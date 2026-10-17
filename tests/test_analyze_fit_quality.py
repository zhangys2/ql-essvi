from scripts.analyze_fit_quality import source_contract_index


def test_source_contract_index_skips_non_objects_and_preserves_raw_positions():
    contracts = [None, {"conId": 101}, {}, "not-a-contract"]

    indexed = source_contract_index(contracts)

    assert indexed == {
        "101": (2, contracts[1]),
        "3": (3, contracts[2]),
    }
