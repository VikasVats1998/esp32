"""DeskLink v1 in Python (same protocol as Shared/DeskLink): pairing, sealed commands. Needs `cryptography`."""
import json
import os
import urllib.error
import urllib.request


def _crypto():
    try:
        from cryptography.hazmat.primitives import hashes, serialization
        from cryptography.hazmat.primitives.asymmetric import ec
        from cryptography.hazmat.primitives.ciphers.aead import AESGCM
        from cryptography.hazmat.primitives.kdf.hkdf import HKDF
        return hashes, serialization, ec, AESGCM, HKDF
    except ImportError:
        raise SystemExit("the 'cryptography' package is missing: run setup.sh / setup.ps1, or: pip install cryptography")


def keypair():
    hashes, ser, ec, _, _ = _crypto()
    priv = ec.generate_private_key(ec.SECP256R1())
    pub = priv.public_key().public_bytes(ser.Encoding.X962, ser.PublicFormat.UncompressedPoint)
    return priv, pub


def derive(priv, peer_pub, client_pub, server_pub):
    hashes, ser, ec, _, HKDF = _crypto()
    peer = ec.EllipticCurvePublicKey.from_encoded_point(ec.SECP256R1(), peer_pub)
    shared = priv.exchange(ec.ECDH(), peer)
    return HKDF(algorithm=hashes.SHA256(), length=32, salt=b"desklink-v1", info=client_pub + server_pub).derive(shared)


def seal(key, challenge, purpose, plain: bytes):
    AESGCM = _crypto()[3]
    iv = os.urandom(12)
    return iv + AESGCM(key).encrypt(iv, plain, challenge + purpose.encode())


def open_(key, challenge, purpose, blob: bytes):
    AESGCM = _crypto()[3]
    return AESGCM(key).decrypt(blob[:12], blob[12:], challenge + purpose.encode())


class DeskError(Exception):
    def __init__(self, msg, status=None):
        super().__init__(msg)
        self.status = status


class Desk:
    def __init__(self, host, creds=None, timeout=4.0):
        self.host, self.creds, self.timeout = host, creds, timeout

    def _http(self, method, path, body=None):
        data = body.encode() if isinstance(body, str) else body
        req = urllib.request.Request(f"http://{self.host}{path}", data=data, method=method, headers={"Content-Type": "application/json"})
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as r:
                return r.status, r.read().decode()
        except urllib.error.HTTPError as e:
            return e.code, e.read().decode()
        except Exception as e:
            raise DeskError(f"{self.host}: {e}")

    def info(self):
        s, t = self._http("GET", "/dl/info")
        if s != 200:
            raise DeskError(f"info: HTTP {s}", s)
        return json.loads(t)

    def pair(self, name="bringup"):
        priv, pub = keypair()
        s, t = self._http("POST", "/dl/pair", json.dumps({"name": name, "pub": pub.hex()}))
        if s != 200:
            raise DeskError(f"pairing refused (HTTP {s}): {t}", s)
        j = json.loads(t)
        key = derive(priv, bytes.fromhex(j["pub"]), pub, bytes.fromhex(j["pub"]))
        self.creds = {"id": j["id"], "key": key.hex()}
        return self.creds

    def call(self, command):
        if not self.creds:
            raise DeskError("not paired")
        key = bytes.fromhex(self.creds["key"])
        s, t = self._http("GET", "/dl/challenge")
        if s != 200:
            raise DeskError(f"challenge: HTTP {s}", s)
        chal = bytes.fromhex(json.loads(t)["c"])
        sealed = seal(key, chal, "cmd", json.dumps(command).encode())
        s, t = self._http("POST", "/dl/cmd", json.dumps({"id": self.creds["id"], "c": chal.hex(), "d": sealed.hex()}))
        if s == 401:
            raise DeskError("the desk rejected our key (pair again)", 401)
        if s == 429:
            raise DeskError("the desk is locked (too many bad messages) or busy", 429)
        if s != 200:
            raise DeskError(f"HTTP {s}", s)
        return json.loads(open_(key, chal, "rsp", bytes.fromhex(json.loads(t)["d"])))

    def security_probe(self):
        """Replay and wrong-key checks (cost the desk 2 of its 5 allowed bad messages per minute)."""
        key = bytes.fromhex(self.creds["key"])
        def chal():
            return bytes.fromhex(json.loads(self._http("GET", "/dl/challenge")[1])["c"])
        c1 = chal()
        body = json.dumps({"id": self.creds["id"], "c": c1.hex(), "d": seal(key, c1, "cmd", b'{"op":"state"}').hex()})
        first, second = self._http("POST", "/dl/cmd", body)[0], self._http("POST", "/dl/cmd", body)[0]
        evil, c2 = os.urandom(32), chal()
        bad = self._http("POST", "/dl/cmd", json.dumps({"id": self.creds["id"], "c": c2.hex(), "d": seal(evil, c2, "cmd", b'{"op":"state"}').hex()}))[0]
        return first == 200 and second == 401, bad == 401
