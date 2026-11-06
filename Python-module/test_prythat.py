# test_prythat.py - pytest tests for the 'prythat' Python module.
#
# Build the module first (see build-launch.sh / build-launch.bat), then run
#     python -m pytest test_prythat.py
# from this directory.

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


def test_bracmat_usable_after_parse_error():
    with pytest.raises(ValueError):
        prythat.HolyGrail("(")
    assert prythat.HolyGrail("2+3") == "5"
