
#include <chrono>
#include <ctime>
#include <docopt/docopt.h>
#include <boost/filesystem.hpp>
#include <boost/regex.hpp>
#include <ratio>

#include "../src/driver.h"

using namespace std::chrono;


void Main(string directory, string jobs_file, string output_file) {

	Logger->debug("groot.cpp - demo function called");
	Driver driver;

	high_resolution_clock::time_point t1 = high_resolution_clock::now();

	std::ifstream metadataFile((boost::filesystem::path{ directory } / boost::filesystem::path{ "metadata.json" }).string());
	json metadata;
	metadataFile >> metadata;
	Logger->debug("groot.cpp (demo) - Successfully read metadata.json file");

	driver.SetContext(metadata, directory);

	high_resolution_clock::time_point t2 = high_resolution_clock::now();
	duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
	Logger->debug("groot.cpp (demo) - Label graph and Zone graphs built");
	Logger->info(fmt::format("Time to build label graph and zone graphs: {}s", time_span.count()));

	long total_ecs = 0;

	if (jobs_file.length()) {
		std::ifstream i(jobs_file);
		json jobs;
		i >> jobs;

		Logger->debug("groot.cpp (demo) - Successfully read jobs.json file");

		for (auto& user_job : jobs) {
			driver.SetJob(user_job);
			Logger->debug(fmt::format("groot.cpp (demo) - Started property checking for {}", string(user_job["Domain"])));
			driver.GenerateECsAndCheckProperties();
			total_ecs += driver.GetECCountForCurrentJob();
			Logger->debug(fmt::format("groot.cpp (demo) - Finished property checking for {} with {} ECs", string(user_job["Domain"]), driver.GetECCountForCurrentJob()));
		}
	}
	else {
		driver.SetJob(string("."));
		Logger->debug(fmt::format("groot.cpp (demo) - Started default property checking"));
		driver.GenerateECsAndCheckProperties();
		total_ecs += driver.GetECCountForCurrentJob();
		Logger->debug(fmt::format("groot.cpp (demo) - Finished default property checking with {} ECs", driver.GetECCountForCurrentJob()));
	}
	t2 = high_resolution_clock::now();
	time_span = duration_cast<duration<double>>(t2 - t1);
	Logger->info(fmt::format("Time to check all user jobs: {}s", time_span.count()));
	Logger->info(fmt::format("Total number of ECs across all jobs: {}", total_ecs));
	driver.WriteViolationsToFile(output_file);
}

string ZoneFileNSMap(string file, json& metadata, set<string>& required_domains, string second_level_tld) {
	std::ifstream infile(file);
	std::string line;
	const boost::regex fieldsregx(";(?=(?:[^\"]*\"[^\"]*\")*(?![^\"]*\"))");
	const boost::regex linesregx("\\r\\n|\\n\\r|\\n|\\r");
	string top_server = "";
	while (std::getline(infile, line))
	{
		boost::sregex_token_iterator ti(line.begin(), line.end(), fieldsregx, -1);
		boost::sregex_token_iterator end2;

		std::vector<std::string> row;
		while (ti != end2) {
			std::string token = ti->str();
			++ti;
			row.push_back(token);
		}
		if (line.back() == ',') {
			// last character was a separator
			row.push_back("");
		}
		if (row.size() == 3) {
			string domain = row[1].substr(0, row[1].size() - 5);
			if (required_domains.find(domain) != required_domains.end()) {
				json tmp = {};
				tmp["FileName"] = row[1];
				tmp["NameServer"] = row[2] + ".";
				metadata["ZoneFiles"].push_back(tmp);
				if (domain == second_level_tld)top_server = row[2] + ".";
			}
		}
	}
	return top_server;
}

void ReadZoneFilesInfo(string file, std::vector<vector<std::string>> &info)
{
    std::ifstream infile(file);
    std::string line;
    const boost::regex fieldsregx(";(?=(?:[^\"]*\"[^\"]*\")*(?![^\"]*\"))");
    const boost::regex linesregx("\\r\\n|\\n\\r|\\n|\\r");
    string top_server = "";
    while (std::getline(infile, line)) {
        boost::sregex_token_iterator ti(line.begin(), line.end(), fieldsregx, -1);
        boost::sregex_token_iterator end2;

        std::vector<std::string> row;
        while (ti != end2) {
            std::string token = ti->str();
            ++ti;
            row.push_back(token);
        }
        if (row.size() == 3) {
            info.push_back(row);
        }
    }
}

string SearchReturnNS(
    std::vector<vector<std::string>> &info,
    json &metadata,
    set<string> &required_domains,
    string second_level_tld)
{
    string top_server = "";
    for (auto &row : info) {
        string domain = row[1].substr(0, row[1].size() - 5);
        if (required_domains.find(domain) != required_domains.end()) {
            json tmp = {};
            tmp["FileName"] = row[1];
            tmp["NameServer"] = row[2] + ".";
            metadata["ZoneFiles"].push_back(tmp);
            if (domain == second_level_tld)
                top_server = row[2] + ".";
        }
	}
    return top_server;
}

void GenerateMetaDataFiles()
{
    string data_path = "C:/Users/sivak/Desktop/Data/";

    json subdomains;
    std::ifstream subdomainsFile(
        (boost::filesystem::path{data_path} / boost::filesystem::path{"2ndLevelTLD-SubZones.json"}).string());
    subdomainsFile >> subdomains;
    Logger->debug(fmt::format("groot.cpp (CensusData) - Successfully read subdomain json"));

    std::vector<vector<std::string>> info;
    ReadZoneFilesInfo(data_path + "NameServer_FileName.csv", info);

    const int thread_count = 16;
    std::thread metadata_producers[thread_count];
    int chunk = (subdomains.size() / thread_count) + thread_count;

    for (int i = 0; i != thread_count; ++i) {
        metadata_producers[i] = thread([i, &chunk, &subdomains, &info, &data_path]() {
            int j = 0;
            int start = i * chunk;
            int end = (i + 1) * chunk;
            for (auto &[second_level_tld, subs] : subdomains.items()) {
                if (start <= j && j<= end) {
                    //Logger->info(fmt::format("Thread {} started with start {} and end {}, stld {}", i, start, end, second_level_tld));
                    json metadata = {};
                    metadata["TopNameServers"] = {};
                    metadata["ZoneFiles"] = {};
                    std::set<string> required_domains;
                    required_domains.insert(string(second_level_tld));
                    for (auto &s : subs) {
                        required_domains.insert(string(s));
                    }
                    string top_server = SearchReturnNS(info, metadata, required_domains, string(second_level_tld));
                    if (top_server.length() > 0) {
                        metadata["TopNameServers"].push_back(top_server);
                        std::ofstream ofs;
                        ofs.open(
                            data_path + "metadata_files/" + string(second_level_tld) + ".json", std::ofstream::out);
                        ofs << metadata.dump(4);
                        ofs.close();
                    }
				}
                j++;
            }
        });
    }
    for (int i = 0; i != thread_count; ++i) {
        metadata_producers[i].join();
    }
}


void CensusData(string second_level_tld, string output_file) {
	string data_path = "C:/Users/sivak/Desktop/Data/";

	json subdomains;
	std::ifstream subdomainsFile((boost::filesystem::path{ data_path } / boost::filesystem::path{ "2ndLevelTLD-SubZones.json" }).string());
	subdomainsFile >> subdomains;
	Logger->debug(fmt::format("groot.cpp (CensusData) - Successfully read subdomain json"));

	json metadata = {};
	metadata["TopNameServers"] = {};
	metadata["ZoneFiles"] = {};
	std::set<string> required_domains;
	for (auto& s : subdomains[second_level_tld]) {
		required_domains.insert(string(s));
	}
	required_domains.insert(second_level_tld);
	string top_server = ZoneFileNSMap(data_path + "NameServer_FileName.csv", metadata, required_domains, second_level_tld);

	if (top_server.length() > 0) {
		metadata["TopNameServers"].push_back(top_server);
		Logger->debug(fmt::format("groot.cpp (CensusData) - Successfully constructed metadata"));

		Driver driver;

		high_resolution_clock::time_point t1 = high_resolution_clock::now();
		driver.SetContext(metadata, data_path + "zone_files");
		Logger->debug("groot.cpp (CensusData) - Label graph and Zone graphs built");

		high_resolution_clock::time_point t2 = high_resolution_clock::now();
		duration<double> time_span = duration_cast<duration<double>>(t2 - t1);
		Logger->info(fmt::format("Time to build label graph and zone graphs: {}s", time_span.count()));


		driver.SetJob(second_level_tld + ".");
		Logger->debug(fmt::format("groot.cpp (CensusData) - Started property checking for {}", second_level_tld));
		driver.GenerateECsAndCheckProperties();
		Logger->debug(fmt::format("groot.cpp (CensusData) - Finished property checking for {} with {} ECs", second_level_tld, driver.GetECCountForCurrentJob()));

		t2 = high_resolution_clock::now();
		time_span = duration_cast<duration<double>>(t2 - t1);
		Logger->info(fmt::format("Total number of ECs: {}", driver.GetECCountForCurrentJob()));
        Logger->info(fmt::format("Total number of vertices across all interpretation graphs: {}", driver.GetInterpretationVerticesCountForCurrentJob()));
		Logger->info(fmt::format("Time to check all user jobs: {}s", time_span.count()));
		driver.WriteViolationsToFile(output_file);
	}
	else {
		Logger->error(fmt::format("groot.cpp (CensusData) - top name server is empty"));
	}
}

static const char USAGE[] =
R"(groot 1.0
   
Groot is a static verification tool for DNS. Groot consumes
a collection of zone files along with a collection of user- 
defined properties and systematically checks if any input to
DNS can lead to a property violation for the properties.

Usage: groot [-hlsv] [--jobs=<jobs_file_as_json>] <zone_directory> [--output=<output_file>]

Options:
  -h --help     Show this help screen.
  -l --log    Print statistics about the current run. 
  -s --stats    Print statistics about the current run. 
  -v --verbose  Print more information. 
  --version     Show groot version.
)";


int main(int argc, const char** argv)
{
	try {
		auto args = docopt::docopt(USAGE, { argv + 1, argv + argc }, true, "groot 1.0");

		/* for (auto const& arg : args) {
			std::cout << arg.first << arg.second << std::endl;
		 }*/
		spdlog::init_thread_pool(8192, 1);

		auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		if (args.find("--stats")->second.asBool()) {
			stdout_sink->set_level(spdlog::level::info);
		}
		else {
			stdout_sink->set_level(spdlog::level::warn);
		}
		stdout_sink->set_pattern("[%x %H:%M:%S.%e] [thread %t] [%^%=7l%$] %v");

		std::vector<spdlog::sink_ptr> sinks{ stdout_sink };

		if (args.find("--log")->second.asBool()) {
			auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("log.txt", true);
			file_sink->set_level(spdlog::level::trace);
			file_sink->set_pattern("[%x %H:%M:%S.%e] [thread %t] [%^%=7l%$] %v");
			sinks.push_back(file_sink);
		}

		auto logger = std::make_shared<spdlog::async_logger>("my_custom_logger", sinks.begin(), sinks.end(), spdlog::thread_pool(), spdlog::async_overflow_policy::block);
		//auto  logger = std::make_shared<spdlog::logger>("my_custom_logger", sinks.begin(), sinks.end());
		logger->flush_on(spdlog::level::trace);

		bool verbose = args.find("--verbose")->second.asBool();
		if (verbose) {
			logger->set_level(spdlog::level::trace);
		}
		else {
			logger->set_level(spdlog::level::debug);
		}
		spdlog::register_logger(logger);

		Logger->bind(spdlog::get("my_custom_logger"));

		string zone_directory;
		string jobs_file;
		auto z = args.find("<zone_directory>");
		if (!z->second)
		{
			Logger->critical(fmt::format("groot.cpp (main) - missing parameter <zone_directory>"));
			cout << USAGE[0];
			exit(EXIT_FAILURE);
		}
		else
		{
			zone_directory = z->second.asString();
		}

		auto p = args.find("--jobs");
		if (p->second)
		{
			jobs_file = p->second.asString();
		}

		p = args.find("--output");
		string output_file = "output.json";
		if (p->second)
		{
			output_file = p->second.asString();
		}
        GenerateMetaDataFiles();
		// TODO: validate that the directory and property files exist
		Main(zone_directory, jobs_file, output_file);
		Logger->debug("groot.cpp (main) - Finished checking all jobs");
		spdlog::shutdown();
		return 0;
	}
	catch (exception& e) {
		cout << "Exception:- " << e.what() << endl;
		spdlog::shutdown();
	}
}