#!/usr/bin/env python3
"""
Metravi 3005P Programmable Power Supply — Dual PSU Data Logger, BigQuery Syncer & Web Control Hub

Features:
- Concurrent USB-Serial communication with multiple Metravi/QJE 3005P units (PSU1 & PSU2)
- Fixed udev device mapping (/dev/metravi_PSU1, /dev/metravi_PSU2)
- Real-time 1-second polling per PSU of voltage, current, setpoints, and CV/CC mode
- Local CSV data logging with daily rotation per device (data/metravi_{device}_{timestamp}.csv)
- Micro-batch streaming to Google BigQuery (grafana-494005.Datas.metravi_power_data) with 'device' column
- GCP Cloud Pub/Sub remote command routing per device
- Interactive Web Control Dashboard on port 5000 with Dual PSU controls
"""

import serial
import serial.tools.list_ports
import time
import csv
import json
import os
import sys
import signal
import logging
import queue
import threading
from datetime import datetime, timezone, timedelta
from pathlib import Path
from flask import Flask, render_template, request, jsonify

# ---------------------------------------------------------------------------
# Logging setup
# ---------------------------------------------------------------------------
logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S",
)
logger = logging.getLogger("metravi-logger")

# Suppress verbose Flask request logs in journal
log = logging.getLogger('werkzeug')
log.setLevel(logging.WARNING)

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------
CONFIG_PATH = Path(__file__).parent / "config.json"

DEFAULT_CONFIG = {
    "devices": [
        {"id": "PSU1", "name": "Metravi PSU 1", "port": "/dev/metravi_PSU1", "fallback_port": "/dev/ttyUSB0"},
        {"id": "PSU2", "name": "Metravi PSU 2", "port": "/dev/metravi_PSU2", "fallback_port": "/dev/ttyUSB1"},
    ],
    "baud_rate": 9600,
    "timeout": 1.5,
    "poll_interval_seconds": 1.0,
    "csv_output_dir": "/home/ossusbio/metravi-logger/data",
    "csv_rotate_hours": 24,
    "retry_delay_seconds": 5,
    "max_retries": -1,
    "bigquery_enabled": True,
    "gcp_project_id": "grafana-494005",
    "gcp_dataset": "Datas",
    "gcp_table": "metravi_power_data",
    "gcp_key_file": "/home/ossusbio/metravi-logger/gcp_key.json",
    "batch_push_seconds": 10,
    "web_port": 5000,
    "pubsub_enabled": True,
    "pubsub_subscription": "metravi-pi-sub",
}


def load_config() -> dict:
    config = DEFAULT_CONFIG.copy()
    if CONFIG_PATH.exists():
        try:
            with open(CONFIG_PATH, "r") as f:
                user_config = json.load(f)
            config.update(user_config)
            logger.info("Loaded config from %s", CONFIG_PATH)
        except Exception as e:
            logger.warning("Failed to load config (%s), using defaults", e)
    return config


# ---------------------------------------------------------------------------
# Serial communication with Metravi (Thread-Safe per device)
# ---------------------------------------------------------------------------
class MetraviPowerSupply:
    def __init__(self, device_id: str, name: str, port: str, fallback_port: str = "", baud_rate: int = 9600, timeout: float = 1.5):
        self.device_id = device_id
        self.name = name
        self.preferred_port = port
        self.fallback_port = fallback_port
        self.port = port
        self.baud_rate = baud_rate
        self.timeout = timeout
        self.serial: serial.Serial | None = None
        self.lock = threading.Lock()
        self.device_model = "Unknown"
        self.connected = False

    def _resolve_port(self) -> str:
        if os.path.exists(self.preferred_port):
            return self.preferred_port
        if self.fallback_port and os.path.exists(self.fallback_port):
            return self.fallback_port
        return self.preferred_port

    def connect(self) -> bool:
        with self.lock:
            self.port = self._resolve_port()
            try:
                if self.serial and self.serial.is_open:
                    self.serial.close()
                self.serial = serial.Serial(
                    port=self.port,
                    baudrate=self.baud_rate,
                    bytesize=serial.EIGHTBITS,
                    parity=serial.PARITY_NONE,
                    stopbits=serial.STOPBITS_ONE,
                    timeout=self.timeout,
                    write_timeout=self.timeout,
                )
                time.sleep(0.2)
                self.serial.reset_input_buffer()
                self.serial.reset_output_buffer()
                self.connected = True
                logger.info("[%s] Serial port %s opened (%d baud)", self.device_id, self.port, self.baud_rate)
                return True
            except serial.SerialException as e:
                self.connected = False
                logger.error("[%s] Failed to open %s: %s", self.device_id, self.port, e)
                return False

    def disconnect(self):
        with self.lock:
            if self.serial and self.serial.is_open:
                try:
                    self.serial.close()
                except Exception:
                    pass
            self.connected = False
            logger.info("[%s] Serial port closed", self.device_id)

    def _send_raw(self, command: str) -> None:
        if not self.serial or not self.serial.is_open:
            return
        payload = (command + "/n").encode("ascii")
        self.serial.write(payload)
        self.serial.flush()
        time.sleep(0.12)

    def _query_locked(self, command: str) -> str | None:
        if not self.serial or not self.serial.is_open:
            return None
        try:
            self.serial.reset_input_buffer()
            self._send_raw(command)
            response = self.serial.readline().decode("ascii", errors="replace").strip()
            return response if response else None
        except Exception as e:
            logger.error("[%s] Serial error querying '%s': %s", self.device_id, command, e)
            return None

    def query(self, command: str) -> str | None:
        with self.lock:
            return self._query_locked(command)

    def send_cmd(self, command: str) -> bool:
        with self.lock:
            try:
                self.serial.reset_input_buffer()
                self._send_raw(command)
                return True
            except Exception as e:
                logger.error("[%s] Serial error sending '%s': %s", self.device_id, command, e)
                return False

    def identify(self) -> str | None:
        name = self.query("*IDN?")
        if name:
            self.device_model = name
        return name

    def set_voltage(self, voltage: float) -> bool:
        v_str = f"{voltage:05.2f}"
        logger.info("[%s] Setting voltage to %s V", self.device_id, v_str)
        return self.send_cmd(f"VSET1:{v_str}")

    def set_current(self, current: float) -> bool:
        i_str = f"{current:05.3f}"
        logger.info("[%s] Setting current limit to %s A", self.device_id, i_str)
        return self.send_cmd(f"ISET1:{i_str}")

    def set_output(self, enable: bool) -> bool:
        cmd = "OUTPUT1" if enable else "OUTPUT0"
        logger.info("[%s] Setting output to %s", self.device_id, "ON" if enable else "OFF")
        return self.send_cmd(cmd)

    def read_full_telemetry(self) -> dict | None:
        """Poll all telemetry fields in one atomic thread-locked pass."""
        with self.lock:
            vout_str = self._query_locked("VOUT1?")
            iout_str = self._query_locked("IOUT1?")
            if vout_str is None and iout_str is None:
                return None

            vset_str = self._query_locked("VSET1?")
            iset_str = self._query_locked("ISET1?")
            st_str = self._query_locked("STATUS?")

            try:
                vout = float(vout_str) if vout_str else 0.0
            except ValueError:
                vout = 0.0

            try:
                iout = float(iout_str) if iout_str else 0.0
            except ValueError:
                iout = 0.0

            try:
                vset = float(vset_str) if vset_str else vout
            except ValueError:
                vset = 0.0

            try:
                iset = float(iset_str) if iset_str else iout
            except ValueError:
                iset = 0.0

            # STATUS format: bit0=CV(1)/CC(0), bit1=Output(1/0), bit2=OCP(1/0)
            mode = "CV"
            output_on = False
            ocp_active = False
            if st_str and len(st_str) >= 3:
                mode = "CV" if st_str[0] == "1" else "CC"
                output_on = st_str[1] == "1"
                ocp_active = st_str[2] == "1"

            ts = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%S.%fZ")
            return {
                "timestamp": ts,
                "device": self.device_id,
                "vout": round(vout, 2),
                "iout": round(iout, 3),
                "vset": round(vset, 2),
                "iset": round(iset, 3),
                "power": round(vout * iout, 2),
                "mode": mode,
                "output_on": output_on,
                "ocp_active": ocp_active,
                "connected": True,
                "port": self.port,
                "device_model": self.device_model,
            }


# ---------------------------------------------------------------------------
# CSV Logger with Daily Rotation (One per device)
# ---------------------------------------------------------------------------
class CSVLogger:
    HEADERS = ["timestamp", "device", "vout", "iout", "mode"]

    def __init__(self, output_dir: str, device_id: str, rotate_hours: int = 24):
        self.output_dir = Path(output_dir)
        self.device_id = device_id
        self.rotate_hours = rotate_hours
        self.current_file = None
        self.csv_writer = None
        self.file_handle = None
        self.file_created_at = None

    def _ensure_dir(self):
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def _should_rotate(self) -> bool:
        if self.file_created_at is None:
            return True
        elapsed = datetime.now() - self.file_created_at
        return elapsed > timedelta(hours=self.rotate_hours)

    def _open_new_file(self):
        self.close()
        self._ensure_dir()
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        filepath = self.output_dir / f"metravi_{self.device_id}_{timestamp}.csv"
        self.file_handle = open(filepath, "w", newline="", buffering=1)
        self.csv_writer = csv.DictWriter(self.file_handle, fieldnames=self.HEADERS)
        self.csv_writer.writeheader()
        self.file_handle.flush()
        self.current_file = filepath
        self.file_created_at = datetime.now()
        logger.info("[%s] Opened new CSV file: %s", self.device_id, filepath)

    def write(self, data: dict):
        if self._should_rotate():
            self._open_new_file()
        try:
            row = {
                "timestamp": data["timestamp"],
                "device": self.device_id,
                "vout": data["vout"],
                "iout": data["iout"],
                "mode": data["mode"],
            }
            self.csv_writer.writerow(row)
            self.file_handle.flush()
        except Exception as e:
            logger.error("[%s] Failed to write CSV row: %s", self.device_id, e)

    def close(self):
        if self.file_handle:
            try:
                self.file_handle.close()
            except Exception:
                pass
            self.file_handle = None
            self.csv_writer = None


# ---------------------------------------------------------------------------
# BigQuery Micro-Batch Uploader (Supports 'device' column)
# ---------------------------------------------------------------------------
class BigQueryUploader:
    def __init__(self, project_id: str, dataset: str, table: str, key_file: str, batch_seconds: int = 10):
        self.project_id = project_id
        self.table_ref = f"{project_id}.{dataset}.{table}"
        self.key_file = key_file
        self.batch_seconds = batch_seconds
        self.queue = queue.Queue()
        self.running = False
        self.thread: threading.Thread | None = None
        self.client = None
        self.total_pushed = 0

    def initialize(self) -> bool:
        try:
            from google.cloud import bigquery
            from google.oauth2 import service_account

            if os.path.exists(self.key_file):
                creds = service_account.Credentials.from_service_account_file(self.key_file)
                self.client = bigquery.Client(project=self.project_id, credentials=creds)
                logger.info("BigQuery client initialized using %s (Table: %s)", self.key_file, self.table_ref)
            else:
                self.client = bigquery.Client(project=self.project_id)
                logger.info("BigQuery client initialized using ADC (Table: %s)", self.table_ref)
            return True
        except Exception as e:
            logger.error("Failed to initialize BigQuery client: %s", e)
            return False

    def start(self):
        if not self.client and not self.initialize():
            logger.warning("BigQuery uploader disabled due to init error")
            return
        self.running = True
        self.thread = threading.Thread(target=self._worker_loop, daemon=True, name="BigQueryUploader")
        self.thread.start()
        logger.info("BigQuery sync worker started (push every %ds)", self.batch_seconds)

    def enqueue(self, row: dict):
        # Enqueue BigQuery fields including 'device'
        bq_row = {
            "timestamp": row["timestamp"],
            "device": row.get("device", "PSU1"),
            "vout": row["vout"],
            "iout": row["iout"],
            "mode": row["mode"],
            "output_on": bool(row.get("output_on", False)),
        }
        self.queue.put(bq_row)

    def _flush_batch(self, batch: list) -> bool:
        if not batch or not self.client:
            return True
        try:
            errors = self.client.insert_rows_json(self.table_ref, batch)
            if errors:
                logger.error("BigQuery insert errors: %s", errors)
                return False
            self.total_pushed += len(batch)
            logger.info("Pushed %d rows to BigQuery (Total: %d)", len(batch), self.total_pushed)
            return True
        except Exception as e:
            logger.warning("Network/BigQuery push failed (%s). Retrying later...", e)
            return False

    def _worker_loop(self):
        pending_batch = []
        last_push_time = time.time()

        while self.running or not self.queue.empty():
            try:
                item = self.queue.get(timeout=1.0)
                pending_batch.append(item)
            except queue.Empty:
                pass

            now = time.time()
            if pending_batch and ((now - last_push_time >= self.batch_seconds) or len(pending_batch) >= 50):
                if self._flush_batch(pending_batch):
                    pending_batch = []
                    last_push_time = now
                else:
                    time.sleep(3)

        if pending_batch:
            self._flush_batch(pending_batch)

    def stop(self):
        self.running = False
        if self.thread and self.thread.is_alive():
            self.thread.join(timeout=5.0)
        logger.info("BigQuery uploader stopped")


# ---------------------------------------------------------------------------
# Google Cloud Pub/Sub Command Listener (Real-Time Cloud Bridge with Device Routing)
# ---------------------------------------------------------------------------
class PubSubListener:
    def __init__(self, project_id: str, subscription_id: str, key_file: str, psus: dict[str, MetraviPowerSupply]):
        self.project_id = project_id
        self.subscription_id = subscription_id
        self.key_file = key_file
        self.psus = psus
        self.subscriber = None
        self.streaming_pull_future = None
        self.running = False

    def initialize(self) -> bool:
        try:
            from google.cloud import pubsub_v1
            from google.oauth2 import service_account

            if os.path.exists(self.key_file):
                creds = service_account.Credentials.from_service_account_file(self.key_file)
                self.subscriber = pubsub_v1.SubscriberClient(credentials=creds)
            else:
                self.subscriber = pubsub_v1.SubscriberClient()
            return True
        except Exception as e:
            logger.error("Failed to initialize Pub/Sub client: %s", e)
            return False

    def _callback(self, message):
        try:
            raw_data = message.data.decode("utf-8")
            logger.info("Received Cloud Pub/Sub command: %s", raw_data)
            payload = json.loads(raw_data)

            target_dev = str(payload.get("device", "PSU1")).upper()
            action = payload.get("action")
            value = payload.get("value")

            # Handle global emergency stop
            if action == "emergency_stop" and (target_dev == "ALL" or target_dev not in self.psus):
                for dev_id, psu in self.psus.items():
                    psu.set_output(False)
                logger.info("Emergency Stop ALL executed via Pub/Sub")
                message.ack()
                return

            # Lookup target device (default to PSU1 if unrecognized)
            target_psu = self.psus.get(target_dev)
            if not target_psu:
                target_psu = list(self.psus.values())[0]
                target_dev = target_psu.device_id

            if action == "set_voltage" and value is not None:
                target_psu.set_voltage(float(value))
            elif action == "set_current" and value is not None:
                target_psu.set_current(float(value))
            elif action == "set_output" and value is not None:
                enable = value in [1, True, "1", "true", "True", "ON", "on"]
                target_psu.set_output(enable)
            elif action == "emergency_stop":
                target_psu.set_output(False)
            else:
                logger.warning("Unknown or invalid Pub/Sub command: %s", payload)

            message.ack()
        except Exception as e:
            logger.error("Error executing Pub/Sub command: %s", e)
            message.ack()

    def start(self):
        if not self.subscriber and not self.initialize():
            logger.warning("Pub/Sub listener disabled due to init error")
            return
        self.running = True
        sub_path = self.subscriber.subscription_path(self.project_id, self.subscription_id)
        self.streaming_pull_future = self.subscriber.subscribe(sub_path, callback=self._callback)
        logger.info("Cloud Pub/Sub listener active on %s", sub_path)

    def stop(self):
        self.running = False
        if self.streaming_pull_future:
            try:
                self.streaming_pull_future.cancel()
            except Exception:
                pass
        logger.info("Pub/Sub listener stopped")


# ---------------------------------------------------------------------------
# Flask Web Dashboard Application (Multi-Device API)
# ---------------------------------------------------------------------------
web_app = Flask(__name__, template_folder=str(Path(__file__).parent / "templates"))

g_psus: dict[str, MetraviPowerSupply] = {}
g_uploader: BigQueryUploader | None = None
g_telemetry: dict[str, dict] = {}


def default_telemetry(device_id: str, name: str) -> dict:
    return {
        "device": device_id,
        "name": name,
        "vout": 0.0,
        "iout": 0.0,
        "vset": 0.0,
        "iset": 0.0,
        "power": 0.0,
        "mode": "--",
        "output_on": False,
        "ocp_active": False,
        "connected": False,
        "port": "",
        "device_model": "Unknown",
        "timestamp": "--",
    }


@web_app.route("/")
def index():
    return render_template("index.html")


@web_app.route("/api/status")
def api_status():
    global g_telemetry, g_psus, g_uploader
    # Top-level backward compatibility with PSU1
    psu1_data = g_telemetry.get("PSU1", default_telemetry("PSU1", "Metravi PSU 1")).copy()
    resp = psu1_data.copy()
    resp["devices"] = g_telemetry
    resp["bq_synced_records"] = g_uploader.total_pushed if g_uploader else 0
    return jsonify(resp)


@web_app.route("/api/set_voltage", methods=["POST"])
def api_set_voltage():
    global g_psus
    req = request.get_json() or {}
    device_id = req.get("device", "PSU1").upper()
    psu = g_psus.get(device_id) or g_psus.get("PSU1")
    if not psu:
        return jsonify({"success": False, "error": "Device not found"}), 404

    voltage = float(req.get("voltage", 0.0))
    voltage = max(0.0, min(30.0, voltage))
    if psu.set_voltage(voltage):
        return jsonify({"success": True, "device": psu.device_id, "vset": voltage})
    return jsonify({"success": False, "error": "Failed to send command"}), 500


@web_app.route("/api/set_current", methods=["POST"])
def api_set_current():
    global g_psus
    req = request.get_json() or {}
    device_id = req.get("device", "PSU1").upper()
    psu = g_psus.get(device_id) or g_psus.get("PSU1")
    if not psu:
        return jsonify({"success": False, "error": "Device not found"}), 404

    current = float(req.get("current", 0.0))
    current = max(0.0, min(5.0, current))
    if psu.set_current(current):
        return jsonify({"success": True, "device": psu.device_id, "iset": current})
    return jsonify({"success": False, "error": "Failed to send command"}), 500


@web_app.route("/api/set_output", methods=["POST"])
def api_set_output():
    global g_psus
    req = request.get_json() or {}
    device_id = req.get("device", "PSU1").upper()
    psu = g_psus.get(device_id) or g_psus.get("PSU1")
    if not psu:
        return jsonify({"success": False, "error": "Device not found"}), 404

    output_state = bool(req.get("output", False))
    if psu.set_output(output_state):
        return jsonify({"success": True, "device": psu.device_id, "output_on": output_state})
    return jsonify({"success": False, "error": "Failed to send command"}), 500


@web_app.route("/api/emergency_stop", methods=["POST"])
def api_emergency_stop():
    global g_psus
    req = request.get_json() or {}
    device_id = req.get("device", "ALL").upper()

    if device_id == "ALL":
        for psu in g_psus.values():
            psu.set_output(False)
        return jsonify({"success": True, "device": "ALL", "output_on": False})

    psu = g_psus.get(device_id)
    if psu:
        psu.set_output(False)
        return jsonify({"success": True, "device": psu.device_id, "output_on": False})
    return jsonify({"success": False, "error": "Device not found"}), 404


# ---------------------------------------------------------------------------
# Worker Thread per Power Supply
# ---------------------------------------------------------------------------
class PSUWorker(threading.Thread):
    def __init__(self, psu: MetraviPowerSupply, csv_logger: CSVLogger, uploader: BigQueryUploader | None, poll_interval: float = 1.0):
        super().__init__(daemon=True, name=f"Worker-{psu.device_id}")
        self.psu = psu
        self.csv_logger = csv_logger
        self.uploader = uploader
        self.poll_interval = poll_interval
        self.running = False
        self.read_count = 0
        self.consecutive_errors = 0

    def run(self):
        self.running = True
        logger.info("[%s] Worker thread started for %s", self.psu.device_id, self.psu.port)

        while self.running:
            start_t = time.monotonic()
            try:
                data = self.psu.read_full_telemetry()
                if data:
                    g_telemetry[self.psu.device_id] = data
                    self.csv_logger.write(data)
                    if self.uploader:
                        self.uploader.enqueue(data)

                    self.read_count += 1
                    self.consecutive_errors = 0

                    if self.read_count == 1 or self.read_count % 15 == 0:
                        logger.info(
                            "[%s] Reading #%d | VOUT=%.2f V | IOUT=%.3f A | Mode=%s | Output=%s",
                            self.psu.device_id,
                            self.read_count,
                            data["vout"],
                            data["iout"],
                            data["mode"],
                            "ON" if data["output_on"] else "OFF",
                        )
                else:
                    self.consecutive_errors += 1
                    if self.consecutive_errors % 10 == 0:
                        logger.warning("[%s] Consecutive read misses: %d", self.psu.device_id, self.consecutive_errors)
                        if self.consecutive_errors >= 20:
                            logger.info("[%s] Reconnecting serial port...", self.psu.device_id)
                            self.psu.connect()
                            self.consecutive_errors = 0

            except Exception as e:
                logger.error("[%s] Error in telemetry poll: %s", self.psu.device_id, e)

            elapsed = time.monotonic() - start_t
            sleep_time = max(0.05, self.poll_interval - elapsed)
            time.sleep(sleep_time)

    def stop(self):
        self.running = False


# ---------------------------------------------------------------------------
# Main Dual-PSU Application
# ---------------------------------------------------------------------------
class MetraviApp:
    def __init__(self):
        global g_psus, g_uploader, g_telemetry
        self.config = load_config()
        self.running = False

        # Initialize BigQuery Uploader
        self.uploader = None
        if self.config.get("bigquery_enabled", True):
            self.uploader = BigQueryUploader(
                project_id=self.config["gcp_project_id"],
                dataset=self.config["gcp_dataset"],
                table=self.config["gcp_table"],
                key_file=self.config["gcp_key_file"],
                batch_seconds=self.config.get("batch_push_seconds", 10),
            )
            g_uploader = self.uploader

        # Parse devices
        raw_devices = self.config.get("devices", [])
        if not raw_devices:
            # Fallback to single device config if old format
            raw_devices = [
                {
                    "id": "PSU1",
                    "name": "Metravi PSU 1",
                    "port": self.config.get("serial_port", "/dev/metravi_PSU1"),
                    "fallback_port": "/dev/ttyUSB0",
                }
            ]

        self.psus: dict[str, MetraviPowerSupply] = {}
        self.csv_loggers: dict[str, CSVLogger] = {}
        self.workers: list[PSUWorker] = []

        baud = int(self.config.get("baud_rate", 9600))
        timeout = float(self.config.get("timeout", 1.5))
        poll_interval = float(self.config.get("poll_interval_seconds", 1.0))
        csv_dir = self.config.get("csv_output_dir", "/home/ossusbio/metravi-logger/data")
        csv_rotate = int(self.config.get("csv_rotate_hours", 24))

        for dev in raw_devices:
            dev_id = str(dev["id"]).upper()
            dev_name = dev.get("name", f"Metravi {dev_id}")
            dev_port = dev.get("port", f"/dev/metravi_{dev_id}")
            dev_fallback = dev.get("fallback_port", "")

            psu = MetraviPowerSupply(
                device_id=dev_id,
                name=dev_name,
                port=dev_port,
                fallback_port=dev_fallback,
                baud_rate=baud,
                timeout=timeout,
            )
            self.psus[dev_id] = psu
            self.csv_loggers[dev_id] = CSVLogger(output_dir=csv_dir, device_id=dev_id, rotate_hours=csv_rotate)
            g_telemetry[dev_id] = default_telemetry(dev_id, dev_name)

        g_psus = self.psus

        # Pub/Sub Listener
        self.pubsub_listener = None
        if self.config.get("pubsub_enabled", True):
            self.pubsub_listener = PubSubListener(
                project_id=self.config["gcp_project_id"],
                subscription_id=self.config.get("pubsub_subscription", "metravi-pi-sub"),
                key_file=self.config["gcp_key_file"],
                psus=self.psus,
            )

    def _signal_handler(self, signum, frame):
        logger.info("Shutdown signal received (%d)...", signum)
        self.running = False

    def run(self):
        signal.signal(signal.SIGINT, self._signal_handler)
        signal.signal(signal.SIGTERM, self._signal_handler)

        logger.info("=" * 60)
        logger.info("Metravi Dual-PSU Control Hub & BigQuery Logger — Starting")
        logger.info("=" * 60)

        # Connect devices
        for dev_id, psu in self.psus.items():
            connected = psu.connect()
            if connected:
                idn = psu.identify()
                logger.info("[%s] Connected to %s: ID=%s", dev_id, psu.port, idn or "Unknown")
            else:
                logger.warning("[%s] Initial connect failed on %s, will retry in worker", dev_id, psu.port)

        # Start BigQuery uploader
        if self.uploader:
            self.uploader.start()

        # Start Cloud Pub/Sub listener
        if self.pubsub_listener:
            self.pubsub_listener.start()

        # Start Flask Web Server
        web_port = int(self.config.get("web_port", 5000))
        flask_thread = threading.Thread(
            target=lambda: web_app.run(host="0.0.0.0", port=web_port, debug=False, use_reloader=False),
            daemon=True,
            name="FlaskWebServer",
        )
        flask_thread.start()
        logger.info("Web Control Hub running on http://0.0.0.0:%d", web_port)

        # Start per-device worker threads
        poll_interval = float(self.config.get("poll_interval_seconds", 1.0))
        for dev_id, psu in self.psus.items():
            worker = PSUWorker(psu, self.csv_loggers[dev_id], self.uploader, poll_interval=poll_interval)
            self.workers.append(worker)
            worker.start()

        self.running = True
        try:
            while self.running:
                time.sleep(0.5)
        except KeyboardInterrupt:
            pass

        # Cleanup
        logger.info("Stopping services...")
        for worker in self.workers:
            worker.stop()
        for psu in self.psus.values():
            psu.disconnect()
        for c in self.csv_loggers.values():
            c.close()
        if self.uploader:
            self.uploader.stop()
        if self.pubsub_listener:
            self.pubsub_listener.stop()
        logger.info("All services stopped cleanly.")
        return 0


if __name__ == "__main__":
    app = MetraviApp()
    sys.exit(app.run())
