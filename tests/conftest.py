import pytest


@pytest.fixture(scope="module")
def essvi():
    import ql_essvi

    return ql_essvi


@pytest.fixture(scope="module")
def np():
    import numpy

    return numpy
