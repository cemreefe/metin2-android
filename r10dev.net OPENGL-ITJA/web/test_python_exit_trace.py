from pathlib import Path
import shutil
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parent.parent


@unittest.skipUnless(shutil.which("em++") and shutil.which("node"), "requires Emscripten and Node")
class PythonExitTraceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.directory = tempfile.TemporaryDirectory()
        directory = Path(cls.directory.name)
        source = (ROOT / "clientsource/ScriptLib/PythonUtils.cpp").read_text()
        helper = source[source.index("void PyPrintException()"):source.index("bool __PyCallClassMemberFunc_ByCString")]
        harness = directory / "exit_trace.cpp"
        harness.write_text("".join([
            '#include <Python.h>\n#include <stdio.h>\n#include <stdarg.h>\n'
            'void TraceError(const char* format, ...) {\n'
            '  va_list args; va_start(args, format); vfprintf(stderr, format, args);\n'
            '  va_end(args); fputc(\'\\n\', stderr);\n}\n',
            helper,
            'int main(int argc, char** argv) {\n'
            '  Py_NoSiteFlag = 1; Py_Initialize();\n'
            '  PyObject* globals = PyDict_New();\n'
            '  PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());\n'
            '  PyObject* result = PyRun_String(argv[1], Py_file_input, globals, globals);\n'
            '  if (!result) PyPrintException();\n'
            '  Py_XDECREF(result); Py_DECREF(globals); Py_Finalize();\n'
            '  return 0;\n}\n'
        ]))
        cls.program = directory / "exit_trace.js"
        subprocess.run([
            "em++", str(harness), "-std=c++11", "-DM2_PORT",
            "-I" + str(ROOT / "Extern/include"),
            "-I" + str(ROOT / "Extern/include/Python2"),
            str(ROOT / "Extern/lib/web/libpython2.7.a"),
            "-pthread", "-sSTACK_SIZE=1048576", "-sENVIRONMENT=node",
            "-sEXIT_RUNTIME=1", "-o", str(cls.program),
        ], check=True)

    @classmethod
    def tearDownClass(cls):
        cls.directory.cleanup()

    def run_python(self, source):
        return subprocess.run(
            ["node", str(self.program), source],
            capture_output=True, text=True, timeout=30,
        )

    def test_zero_exit_preserves_origin_before_shutdown(self):
        result = self.run_python("import sys\ndef request_exit():\n    sys.exit(0)\nrequest_exit()\n")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("Python requested client shutdown (SystemExit)", result.stderr)
        self.assertIn("in request_exit", result.stderr)
        self.assertIn("SystemExit: 0", result.stderr)

    def test_nonzero_exit_code_is_unchanged(self):
        result = self.run_python("raise SystemExit(7)")
        self.assertEqual(result.returncode, 7, result.stderr)
        self.assertIn("SystemExit: 7", result.stderr)

    def test_regular_exceptions_keep_normal_handling(self):
        result = self.run_python("raise RuntimeError('missing icon')")
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertIn("RuntimeError: missing icon", result.stderr)
        self.assertNotIn("Python requested client shutdown", result.stderr)


if __name__ == "__main__":
    unittest.main()
