# test_bracmat.py - pytest tests for the 'bracmat' Python package.
#
# Build the module first (see build-launch.sh / build-launch.bat), then run
#     python -m pytest test_bracmat.py
# from this directory.

import io
import threading
import time

import pytest

bracmat = pytest.importorskip("bracmat")


@pytest.fixture(scope="module", autouse=True)
def session():
    # Bracmat has global state: initialise once for all tests in this file.
    bracmat.init()
    yield
    bracmat.final()


def test_simple_evaluation():
    assert bracmat.HolyGrail("1+1") == "2"


def test_nii_returns_python_value():
    assert bracmat.HolyGrail('Ni!$"3.9+7.123"') == "11.023"


def test_nii_result_survives_repeated_calls():
    # The string returned by Ni!$ must stay valid until Bracmat has copied it.
    assert bracmat.HolyGrail('Ni!$"[1,2]" Ni!$"{\'a\':\'b\'}"') == '[1, 2] {"a": "b"}'


def test_nii_large_result():
    # Length of the JSON string: 100000 x's plus two quotes.
    assert bracmat.HolyGrail('Ni!$"\'x\'*100000":?x&@(!x:? [?n)&!n') == "100002"


def test_nii_exception_makes_call_fail(capsys):
    assert bracmat.HolyGrail('Ni!$"1/0"&yes|failed') == "failed"
    assert "ZeroDivisionError" in capsys.readouterr().err


def test_nii_unserialisable_value_makes_call_fail(capsys):
    assert bracmat.HolyGrail('Ni!$"object()"&yes|failed') == "failed"
    assert "not JSON serializable" in capsys.readouterr().err


def test_ni_exception_does_not_stop_evaluation(capsys):
    assert bracmat.HolyGrail('Ni$"raise RuntimeError(\'boom\')"&after') == "after"
    assert "RuntimeError: boom" in capsys.readouterr().err


def test_parse_error_raises_value_error():
    with pytest.raises(ValueError):
        bracmat.HolyGrail("(")


def test_non_latin1_input_and_output():
    # Input used to be encoded as ISO 8859-1, so this raised UnicodeEncodeError.
    assert bracmat.HolyGrail('"Ωμέγα ∑ 日本"') == "Ωμέγα ∑ 日本"
    assert bracmat.HolyGrail("upp$ωμέγα") == "ΩΜΈΓΑ"


def test_invalid_utf8_round_trips():
    # chr$200 is a lone byte 0xC8, not valid UTF-8.
    s = bracmat.HolyGrail("chr$200")
    assert s == "\udcc8"
    assert bracmat.HolyGrail('"' + s + '"') == s


def test_put_writes_utf8_as_text(capsys):
    bracmat.HolyGrail('put$"Ëlvis 日本\n"')
    assert capsys.readouterr().out == "Ëlvis 日本\n"


def test_get_reads_utf8_from_stdin(monkeypatch):
    monkeypatch.setattr("sys.stdin", io.StringIO("Ëlvis\n"))
    assert bracmat.HolyGrail("get'") == "Ëlvis"


def test_nii_returns_non_ascii_unescaped():
    # json.dumps escaped non-ASCII as \uXXXX.
    assert bracmat.HolyGrail('Ni!$"\'Ωμέγα\'"') == '"Ωμέγα"'


def test_ni_variable_persists_between_calls():
    # exec used a throwaway locals() dict, so 'a' was lost after the call.
    bracmat.HolyGrail('Ni$"a = \'ebbenhout\'"')
    assert bracmat.HolyGrail('Ni!$"a"') == '"ebbenhout"'
    bracmat.HolyGrail('Ni$"a = a.upper()"')
    assert bracmat.namespace()["a"] == "EBBENHOUT"


def test_ni_function_definition_persists():
    bracmat.HolyGrail('Ni$"def sq(x):\n    return x * x\n"')
    assert bracmat.HolyGrail('Ni!$"sq(7)"') == "49"


def test_ni_does_not_see_bracmat_internals():
    # The namespace used to be bracmat's own module globals.
    assert bracmat.HolyGrail('Ni!$"json.__name__"&yes|no') == "no"


def test_init_with_caller_namespace():
    ns = {"b": 41}
    bracmat.final()
    try:
        bracmat.init(ns)
        assert bracmat.HolyGrail('Ni!$"b+1"') == "42"
        bracmat.HolyGrail('Ni$"c = b * 2"')
        assert ns["c"] == 82
    finally:
        bracmat.final()
        bracmat.init()


def test_init_rejects_non_dict_namespace():
    bracmat.final()
    try:
        with pytest.raises(TypeError):
            bracmat.init([])
    finally:
        bracmat.init()


def test_holygrail_after_final_raises():
    bracmat.final()
    try:
        with pytest.raises(RuntimeError, match="not initialised"):
            bracmat.HolyGrail("1+1")
    finally:
        bracmat.init()
    assert bracmat.HolyGrail("1+1") == "2"


def test_init_twice_raises():
    with pytest.raises(RuntimeError, match="already initialised"):
        bracmat.init()
    assert bracmat.HolyGrail("1+1") == "2"


def test_final_twice_is_harmless():
    bracmat.final()
    bracmat.final()
    bracmat.init()
    assert bracmat.HolyGrail("1+1") == "2"


def test_reentrant_holygrail_is_refused(capsys):
    assert bracmat.HolyGrail(
        'Ni!$"__import__(\'bracmat\').HolyGrail(\'1+1\')"&yes|refused') == "refused"
    assert "cannot be called while Bracmat is evaluating" in capsys.readouterr().err
    assert bracmat.HolyGrail("1+1") == "2"


def test_final_from_ni_is_refused(capsys):
    assert bracmat.HolyGrail('Ni$"__import__(\'bracmat\').final()"&after') == "after"
    assert "cannot be called while Bracmat is evaluating" in capsys.readouterr().err
    assert bracmat.HolyGrail("1+1") == "2"


def test_version_matches_bracmat():
    # setup.py takes the version from src/potu.c; !v is Bracmat's own.
    assert bracmat.__version__ != "unknown"
    assert ("version " + bracmat.__version__ + " ") in bracmat.evaluate("!v")


def test_evaluate_is_alias_of_holygrail():
    assert bracmat.evaluate is bracmat.HolyGrail
    assert bracmat.evaluate("3*4") == "12"


def test_non_atom_result_is_printed_form():
    assert bracmat.evaluate("a+b+a") == "2*a+b"


def test_context_manager():
    ns = {}
    bracmat.final()
    try:
        with bracmat.Session(ns) as b:
            assert b.evaluate('Ni$"x = 6 * 7"&Ni!$"x"') == "42"
            assert b.namespace is ns and ns["x"] == 42
        with pytest.raises(RuntimeError, match="not initialised"):
            bracmat.evaluate("1")
    finally:
        bracmat.init()


def test_context_manager_calls_final_on_exception():
    bracmat.final()
    try:
        with pytest.raises(KeyError):
            with bracmat.Session():
                raise KeyError("x")
        with pytest.raises(RuntimeError, match="not initialised"):
            bracmat.evaluate("1")
    finally:
        bracmat.init()


def test_gil_released_during_evaluation():
    # A pure Bracmat loop (no callbacks) that takes a few tenths of a second.
    results = {}

    def worker():
        results["value"] = bracmat.evaluate("0:?i&whl'(!i+1:?i:<3000000)&!i")

    t = threading.Thread(target=worker)
    t.start()
    # If evaluate() held the GIL, this thread could not run until it ended.
    ticks = 0
    while t.is_alive():
        ticks += 1
        time.sleep(0.001)
    t.join()
    assert results["value"] == "3000000"
    assert ticks > 5


def test_second_thread_refused_while_busy():
    # The worker's evaluation blocks inside Ni$ until 'go' is set, so the
    # main thread's call is certain to happen while Bracmat is busy.
    ns = bracmat.namespace()
    ns["started"] = threading.Event()
    ns["go"] = threading.Event()
    results = {}

    def worker():
        results["value"] = bracmat.evaluate('Ni$"started.set(); go.wait(10)"&done')

    t = threading.Thread(target=worker)
    t.start()
    try:
        assert ns["started"].wait(10)
        with pytest.raises(RuntimeError, match="another thread"):
            bracmat.evaluate("1")
    finally:
        ns["go"].set()
        t.join()
    assert results["value"] == "done"


def test_bracmat_usable_after_parse_error():
    with pytest.raises(ValueError):
        bracmat.HolyGrail("(")
    assert bracmat.HolyGrail("2+3") == "5"
