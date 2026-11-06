# SPDX-License-Identifier: GPL-3.0-or-later
"""Zrzut AirBridge na atrapie HTTP: dopytywanie o zadanie, zapis poza repo, podsumowanie bez wyników i numeru seryjnego."""
import http.server
import json
import pathlib
import sys
import threading

import pytest

sys.path.insert(0, str(pathlib.Path(__file__).parent))
import zrzut_airbridge as z  # noqa: E402


class Atrapa(http.server.BaseHTTPRequestHandler):
    metody = []

    def log_message(self, *a):
        pass

    def odp(self, kod, obj):
        b = json.dumps(obj).encode()
        self.send_response(kod)
        self.send_header("Content-Length", str(len(b)))
        self.end_headers()
        self.wfile.write(b)

    def do_GET(self):
        Atrapa.metody.append(self.command)
        p = self.path
        if p == "/api/status":
            return self.odp(200, {"version": "v2.3.0", "pna": "AirSense_10_AutoSet", "srn": "TAJNY123", "mode": "AutoSet"})
        if "action=days" in p:
            return self.odp(202, {"job": "edf:1", "pending": True})
        if "job=edf%3A1" in p or "job=edf:1" in p:
            return self.odp(200, {"days": [{"day": 20756}, {"day": 20757}]})
        if "day=" in p:
            return self.odp(200, {"present": True, "partial": False, "ahi": 42.5, "channels": [{"name": "Press"}],
                                  "settings": {"available": True, "fields": [{"tag": "MOP", "value": "AutoSet"}, {"tag": "MPA", "value": "15"}]}})
        self.odp(404, {})


@pytest.fixture
def adres():
    s = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Atrapa)
    threading.Thread(target=s.serve_forever, daemon=True).start()
    yield f"127.0.0.1:{s.server_address[1]}"
    s.shutdown()


def test_zrzut_na_atrapie(adres, tmp_path):
    Atrapa.metody.clear()
    assert z.main(["--adres", adres, "--konto", "", "--wyjscie", str(tmp_path / "wy")]) == 0
    wy = tmp_path / "wy"
    assert json.loads((wy / "lista-nocy.json").read_text())["days"][0]["day"] == 20756
    assert json.loads((wy / "noc-20757.json").read_text())["ahi"] == 42.5  # surowa odpowiedź zachowana
    pods = (wy / "podsumowanie.txt").read_text()
    assert "AutoSet" in pods and "MPA" in pods and "AirSense_10_AutoSet" in pods
    assert "42.5" not in pods and "TAJNY123" not in pods  # bez wyników i bez numeru seryjnego
    assert set(Atrapa.metody) == {"GET"}  # tylko odczyt


def test_kanaly_jako_obiekt():  # kształt z prawdziwego AirBridge (05.11.2026)
    assert z.podsumuj_noc({"channels": {"Press": {}, "Leak": {}}})["kanaly"] == ["Press", "Leak"]


def test_odmowa_w_repozytorium(adres, tmp_path):
    (tmp_path / ".git").write_text("gitdir: x")  # drzewo robocze git: .git jest plikiem
    with pytest.raises(SystemExit, match="Odmowa"):
        z.main(["--adres", adres, "--konto", "", "--wyjscie", str(tmp_path / "wy")])


def test_brak_polaczenia_po_ludzku(tmp_path):
    with pytest.raises(SystemExit, match="Nie mogę połączyć"):
        z.main(["--adres", "127.0.0.1:9", "--konto", "", "--wyjscie", str(tmp_path / "wy")])
