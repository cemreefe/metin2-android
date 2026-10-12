from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
import stage_client_locales
import m2dev_proto_plain


class LocaleStagingTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.root = Path(self.directory.name)
        for name in ("en", "tr"):
            (self.root / "locale" / name).mkdir(parents=True)

    def tearDown(self):
        self.directory.cleanup()

    def write_proto(self, locale, kind):
        sodium = m2dev_proto_plain.load_sodium()
        key, nonce = m2dev_proto_plain.derive(sodium, m2dev_proto_plain.KEYS[kind])
        stride = 236 if kind == "item" else 335
        start = 8 if kind == "item" else 4
        row = bytearray(stride)
        struct.pack_into("<I", row, 0, 10)
        name = "Kılıç" if kind == "item" else "Yabani Köpek"
        row[start + 65:start + 130] = name.encode("utf-8").ljust(65, b"\0")
        row[start + 130:] = b"\x42" * (stride - start - 130)
        payload = m2dev_proto_plain.lzo_compress(bytes(row))
        body = b"MCOZ" + payload
        body = body.ljust((len(body) + 7) // 8 * 8, b"\0")
        encrypted = m2dev_proto_plain.ctypes.create_string_buffer(len(body))
        sodium.crypto_stream_xchacha20_xor(
            encrypted, body, m2dev_proto_plain.ctypes.c_ulonglong(len(body)), nonce, key)
        block = struct.pack("<4sIII", b"MCOZ", len(body), len(payload), stride) + encrypted.raw
        path = self.root / "locale" / locale / (kind + "_proto")
        header = (struct.pack("<4sIIII", b"MIPX", 1, stride, 1, len(block))
                  if kind == "item" else struct.pack("<4sII", b"MMPT", 1, len(block)))
        path.write_bytes(header + block)
        return path

    def test_all_locales_and_both_proto_kinds_are_converted(self):
        protos = [self.write_proto(locale, kind)
                  for locale in ("en", "tr") for kind in ("item", "mob")]
        stage_client_locales.stage_locales(self.root, "en")
        for proto in protos:
            data = proto.read_bytes()
            offset = 20 if data[:4] == b"MIPX" else 12
            magic, encrypted, size, real = struct.unpack_from("<4sIII", data, offset)
            self.assertEqual((magic, encrypted), (b"MCOZ", 0))
            row = m2dev_proto_plain.lzo_decompress(data[offset + 20:offset + 20 + size], real)
            start = 8 if proto.name == "item_proto" else 4
            name = "Kılıç" if proto.name == "item_proto" else "Yabani Köpek"
            encoding = "cp1254" if proto.parent.name == "tr" else "utf-8"
            self.assertEqual(row[start + 65:start + 130],
                             name.encode(encoding).ljust(65, b"\0"))
            self.assertEqual(row[start + 130:], b"\x42" * (real - start - 130))

    def test_classic_tea_proto_is_also_converted(self):
        key = m2dev_proto_plain.KEYS["item"]
        y, z = struct.unpack("<II", b"MCOZdata")
        total = 0
        for _ in range(32):
            y = (y + ((((z << 4) ^ (z >> 5)) + z) ^ (total + key[total & 3]))) & 0xFFFFFFFF
            total = (total + 0x9E3779B9) & 0xFFFFFFFF
            z = (z + ((((y << 4) ^ (y >> 5)) + y) ^ (total + key[(total >> 11) & 3]))) & 0xFFFFFFFF
        block = struct.pack("<4sIII", b"MCOZ", 8, 4, 4) + struct.pack("<II", y, z)
        path = self.root / "locale/tr/item_proto"
        path.write_bytes(struct.pack("<4sII", b"MIPT", 1, len(block)) + block)
        output = path.with_suffix(".converted")
        m2dev_proto_plain.convert(path, output, "item")
        data = output.read_bytes()
        self.assertEqual(struct.unpack_from("<4sI", data, 12), (b"MCOZ", 0))
        self.assertEqual(data[28:], b"MCOZdata")

    def test_turkish_text_is_losslessly_encoded(self):
        path = self.root / "locale/tr/locale_interface.txt"
        text = "LOGIN_SELECT_EXIT\tÇıkış\nTITLE\tŞehir seçimi: İğdır\n"
        path.write_text(text, encoding="utf-8")
        stage_client_locales.stage_locales(self.root, "tr")
        self.assertEqual(path.read_bytes().decode("cp1254"), text)
        self.assertEqual((self.root / "loca.cfg").read_text(), "1254 tr\n")

    def test_existing_windows_turkish_bytes_are_preserved(self):
        path = self.root / "locale/tr/skilldesc.txt"
        raw = "Işık ve Şifa".encode("cp1254")
        path.write_bytes(raw)
        stage_client_locales.stage_locales(self.root, "en")
        self.assertEqual(path.read_bytes(), raw)

    def test_staging_is_idempotent(self):
        self.write_proto("tr", "item")
        path = self.root / "locale/tr/locale_interface.txt"
        path.write_text("EXIT\tÇıkış\n", encoding="utf-8")
        stage_client_locales.stage_locales(self.root, "tr")
        before = {p.relative_to(self.root): p.read_bytes()
                  for p in self.root.rglob("*") if p.is_file()}
        stage_client_locales.stage_locales(self.root, "tr")
        after = {p.relative_to(self.root): p.read_bytes()
                 for p in self.root.rglob("*") if p.is_file()}
        self.assertEqual(before, after)

    def test_unrepresentable_text_is_not_silently_damaged(self):
        path = self.root / "locale/tr/locale_interface.txt"
        raw = "TITLE\t🙂\n".encode("utf-8")
        path.write_bytes(raw)
        with self.assertRaises(UnicodeEncodeError):
            stage_client_locales.stage_locales(self.root, "tr")
        self.assertEqual(path.read_bytes(), raw)

    def test_other_locale_text_is_unchanged(self):
        path = self.root / "locale/en/locale_interface.txt"
        path.write_bytes(b"TITLE\tServer\n")
        stage_client_locales.stage_locales(self.root, "en")
        self.assertEqual(path.read_bytes(), b"TITLE\tServer\n")
        self.assertEqual((self.root / "loca.cfg").read_text(), "1252 en\n")


@unittest.skipUnless(shutil.which("g++"), "requires g++")
class HotLocaleCodePageTest(unittest.TestCase):
    def test_real_locale_switch_updates_renderer_and_service(self):
        source = (TOOLS.parents[1] / "clientsource/UserInterface/Locale.cpp").read_bytes().decode("latin1")
        function = source[source.index("void LocaleService_ForceSetLocale("):
                          source.index("#if defined(LOCALE_SERVICE_GLOBAL)")]
        with tempfile.TemporaryDirectory() as directory:
            directory = Path(directory)
            harness = directory / "locale.cpp"
            harness.write_text(
                "#include <cstring>\n#include <strings.h>\n#include <string>\n"
                "#include <cassert>\n#define stricmp strcasecmp\n"
                "char MULTI_LOCALE_NAME[256], MULTI_LOCALE_PATH[256];\n"
                "unsigned MULTI_LOCALE_CODE=1252, rendererCodePage=1252;\n"
                "std::string __SECURITY_KEY_STRING__;\n"
                "bool SetDefaultCodePage(unsigned code) { rendererCodePage=code; return true; }\n"
                + function +
                "\nint main() {\n"
                'LocaleService_ForceSetLocale("tr", "locale/tr");\n'
                "assert(MULTI_LOCALE_CODE==1254 && rendererCodePage==1254);\n"
                'assert(!strcmp(MULTI_LOCALE_PATH, "locale/tr"));\n'
                'LocaleService_ForceSetLocale("en", "locale/en");\n'
                "assert(MULTI_LOCALE_CODE==1252 && rendererCodePage==1252);\n"
                'LocaleService_ForceSetLocale("tr", "locale/tr");\n'
                "assert(MULTI_LOCALE_CODE==1254 && rendererCodePage==1254);\n"
                "}\n", encoding="utf-8")
            binary = directory / "locale"
            subprocess.run(["g++", "-std=c++11", "-DM2_PORT", str(harness), "-o", str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == "__main__":
    unittest.main()
