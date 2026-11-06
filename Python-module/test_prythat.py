# test_prythat.py - pytest tests for the 'prythat' Python module.
#
# Build the module first (see build-launch.sh / build-launch.bat), then run
#     python -m pytest test_prythat.py
# from this directory.

import io
import threading
import time

import pytest

prythat = pytest.importorskip("prythat")


@pytest.fixture(scope="module", autouse=True)
def bracmat():
    # Bracmat has global state: initialise once for all tests in this file.
    prythat.init()
    yield
    prythat.final()


def test_simple_evaluation():
    assert prythat.HolyGrail("1+1") == "2"


def test_nii_returns_python_value():
    assert prythat.HolyGrail('Ni!$"3.9+7.123"') == "11.023"


def test_nii_result_survives_repeated_calls():
    # The string returned by Ni!$ must stay valid until Bracmat has copied it.
    assert prythat.HolyGrail('Ni!$"[1,2]" Ni!$"{\'a\':\'b\'}"') == '[1, 2] {"a": "b"}'


def test_nii_large_result():
    # Length of the JSON string: 100000 x's plus two quotes.
    assert prythat.HolyGrail('Ni!$"\'x\'*100000":?x&@(!x:? [?n)&!n') == "100002"


def test_nii_exception_makes_call_fail(capsys):
    assert prythat.HolyGrail('Ni!$"1/0"&yes|failed') == "failed"
    assert "ZeroDivisionError" in capsys.readouterr().err


def test_nii_unserialisable_value_makes_call_fail(capsys):
    assert prythat.HolyGrail('Ni!$"object()"&yes|failed') == "failed"
    assert "not JSON serializable" in capsys.readouterr().err


def test_ni_exception_does_not_stop_evaluation(capsys):
    assert prythat.HolyGrail('Ni$"raise RuntimeError(\'boom\')"&after') == "after"
    assert "RuntimeError: boom" in capsys.readouterr().err


def test_parse_error_raises_value_error():
    with pytest.raises(ValueError):
        prythat.HolyGrail("(")


def test_non_latin1_input_and_output():
    # Input used to be encoded as ISO 8859-1, so this raised UnicodeEncodeError.
    assert prythat.HolyGrail('"Ωμέγα ∑ 日本"') == "Ωμέγα ∑ 日本"
    assert prythat.HolyGrail("upp$ωμέγα") == "ΩΜΈΓΑ"


def test_invalid_utf8_round_trips():
    # chr$200 is a lone byte 0xC8, not valid UTF-8.
    s = prythat.HolyGrail("chr$200")
    assert s == "\udcc8"
    assert prythat.HolyGrail('"' + s + '"') == s


def test_put_writes_utf8_as_text(capsys):
    prythat.HolyGrail('put$"Ëlvis 日本\n"')
    assert capsys.readouterr().out == "Ëlvis 日本\n"


def test_get_reads_utf8_from_stdin(monkeypatch):
    monkeypatch.setattr("sys.stdin", io.StringIO("Ëlvis\n"))
    assert prythat.HolyGrail("get'") == "Ëlvis"


def test_nii_returns_non_ascii_unescaped():
    # json.dumps escaped non-ASCII as \uXXXX.
    assert prythat.HolyGrail('Ni!$"\'Ωμέγα\'"') == '"Ωμέγα"'


def test_ni_variable_persists_between_calls():
    # exec used a throwaway locals() dict, so 'a' was lost after the call.
    prythat.HolyGrail('Ni$"a = \'ebbenhout\'"')
    assert prythat.HolyGrail('Ni!$"a"') == '"ebbenhout"'
    prythat.HolyGrail('Ni$"a = a.upper()"')
    assert prythat.namespace()["a"] == "EBBENHOUT"


def test_ni_function_definition_persists():
    prythat.HolyGrail('Ni$"def sq(x):\n    return x * x\n"')
    assert prythat.HolyGrail('Ni!$"sq(7)"') == "49"


def test_ni_does_not_see_prythat_internals():
    # The namespace used to be prythat's own module globals.
    assert prythat.HolyGrail('Ni!$"json.__name__"&yes|no') == "no"


def test_init_with_caller_namespace():
    ns = {"b": 41}
    prythat.final()
    try:
        prythat.init(ns)
        assert prythat.HolyGrail('Ni!$"b+1"') == "42"
        prythat.HolyGrail('Ni$"c = b * 2"')
        assert ns["c"] == 82
    finally:
        prythat.final()
        prythat.init()


def test_init_rejects_non_dict_namespace():
    prythat.final()
    try:
        with pytest.raises(TypeError):
            prythat.init([])
    finally:
        prythat.init()


def test_holygrail_after_final_raises():
    prythat.final()
    try:
        with pytest.raises(RuntimeError, match="not initialised"):
            prythat.HolyGrail("1+1")
    finally:
        prythat.init()
    assert prythat.HolyGrail("1+1") == "2"


def test_init_twice_raises():
    with pytest.raises(RuntimeError, match="already initialised"):
        prythat.init()
    assert prythat.HolyGrail("1+1") == "2"


def test_final_twice_is_harmless():
    prythat.final()
    prythat.final()
    prythat.init()
    assert prythat.HolyGrail("1+1") == "2"


def test_reentrant_holygrail_is_refused(capsys):
    assert prythat.HolyGrail(
        'Ni!$"__import__(\'prythat\').HolyGrail(\'1+1\')"&yes|refused') == "refused"
    assert "cannot be called while Bracmat is evaluating" in capsys.readouterr().err
    assert prythat.HolyGrail("1+1") == "2"


def test_final_from_ni_is_refused(capsys):
    assert prythat.HolyGrail('Ni$"__import__(\'prythat\').final()"&after') == "after"
    assert "cannot be called while Bracmat is evaluating" in capsys.readouterr().err
    assert prythat.HolyGrail("1+1") == "2"


def test_evaluate_is_alias_of_holygrail():
    assert prythat.evaluate is prythat.HolyGrail
    assert prythat.evaluate("3*4") == "12"


def test_non_atom_result_is_printed_form():
    assert prythat.evaluate("a+b+a") == "2*a+b"


def test_context_manager():
    ns = {}
    prythat.final()
    try:
        with prythat.Bracmat(ns) as b:
            assert b.evaluate('Ni$"x = 6 * 7"&Ni!$"x"') == "42"
            assert b.namespace is ns and ns["x"] == 42
        with pytest.raises(RuntimeError, match="not initialised"):
            prythat.evaluate("1")
    finally:
        prythat.init()


def test_context_manager_calls_final_on_exception():
    prythat.final()
    try:
        with pytest.raises(KeyError):
            with prythat.Bracmat():
                raise KeyError("x")
        with pytest.raises(RuntimeError, match="not initialised"):
            prythat.evaluate("1")
    finally:
        prythat.init()


def test_gil_released_during_evaluation():
    # A pure Bracmat loop (no callbacks) that takes a few tenths of a second.
    results = {}

    def worker():
        results["value"] = prythat.evaluate("0:?i&whl'(!i+1:?i:<3000000)&!i")

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
    ns = prythat.namespace()
    ns["started"] = threading.Event()
    ns["go"] = threading.Event()
    results = {}

    def worker():
        results["value"] = prythat.evaluate('Ni$"started.set(); go.wait(10)"&done')

    t = threading.Thread(target=worker)
    t.start()
    try:
        assert ns["started"].wait(10)
        with pytest.raises(RuntimeError, match="another thread"):
            prythat.evaluate("1")
    finally:
        ns["go"].set()
        t.join()
    assert results["value"] == "done"


def test_bracmat_usable_after_parse_error():
    with pytest.raises(ValueError):
        prythat.HolyGrail("(")
    assert prythat.HolyGrail("2+3") == "5"
