from pathlib import Path
import unittest

from render_local import render, validate


class RenderLocalTest(unittest.TestCase):
    def test_actual_template_loads_versioned_engine(self):
        html = render(Path(__file__).with_name("local.html").read_text(), "test-build")
        self.assertIn('<script async src="metin2_web.js?v=test-build"></script>', html)
        self.assertIn("var M2V = 'test-build';", html)
        self.assertIn('m2pack.js?v=test-build', html)
        self.assertNotIn("{{{", html)

    def test_rejects_unexpanded_deployment(self):
        with self.assertRaises(ValueError):
            validate("{{{ SCRIPT }}}")

    def test_rejects_missing_script(self):
        with self.assertRaises(ValueError):
            validate("<html></html>")

    def test_rejects_ungated_engine_script(self):
        with self.assertRaises(ValueError):
            validate('<script async src="metin2_web.js?v=test-build"></script>')

    def test_rejects_missing_template_placeholder(self):
        with self.assertRaises(ValueError):
            render("<html></html>", "test-build")

    def test_rejects_unsafe_version(self):
        with self.assertRaises(ValueError):
            render("{{{ SCRIPT }}}", '"><script>')


if __name__ == "__main__":
    unittest.main()
