#include "driver-test.h"
#include <boost/test/unit_test.hpp>

BOOST_AUTO_TEST_SUITE(BankcardTestSuite)

BOOST_AUTO_TEST_CASE(bankcard_test)
{
    Driver driver;
    DriverTest dt;
    boost::filesystem::path directory("TestFiles");
    std::ifstream metadataFile((directory / "bankcardExample" / "zone_files" / "metadata.json").string());
    json metadata;
    metadataFile >> metadata;

    std::ifstream i((directory / "bankcardExample" / "jobs.json").string());
    json j;
    i >> j;

    long total_rrs_parsed = driver.SetContext(metadata, (directory / "bankcardExample" / "zone_files").string(), false);
    BOOST_TEST(22 == total_rrs_parsed);
    auto types_to_count = dt.GetTypeToCountMap(driver);
    BOOST_TEST(2 == types_to_count["DNAME"]);

    for (auto &user_job : j) {
        driver.SetJob(user_job);
        driver.GenerateECsAndCheckProperties();
    }
    
    BOOST_TEST(1 == dt.GetNumberofViolations(driver));

}

BOOST_AUTO_TEST_SUITE_END()