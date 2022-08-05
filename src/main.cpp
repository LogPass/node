#include "pch.h"

#include "api/api.h"
#include "blockchain/blockchain.h"
#include "communication/communication.h"
#include "database/database.h"
#include "filesystem/filesystem.h"

using namespace logpass;

int main(int argc, char* argv[])
{
#ifdef NDEBUG
    const std::string buildType = "RELEASE";
#else
    const std::string buildType = "DEBUG";
#endif
    std::cout << "Logpass node " << (uint16_t)kVersionMajor << "." << (uint16_t)kVersionMinor <<
        " (" << __DATE__ << ") [" << buildType << "]" << std::endl;

    // prepare options
    program_options::options_description options("General options");
    options.add_options()
        ("help", "shows help message")
        ("print-environment-variables", "print list of all available environment variables")
        ("config", program_options::value<std::string>()->default_value("config.ini"), "path to config (.ini) file")
        ("dir", program_options::value<std::string>()->default_value("."), "directory for node filesystem")
        ("log-level", program_options::value<boost::log::trivial::severity_level>()
         ->default_value(boost::log::trivial::info), "log level to output");

    options.add(DatabaseOptions::getOptionsDescription());
    options.add(BlockchainOptions::getOptionsDescription());
    options.add(CommunicationOptions::getOptionsDescription());
    options.add(ApiOptions::getOptionsDescription());

    // parse options
    program_options::variables_map vm;
    try {
        // parse command line
        program_options::store(program_options::parse_command_line(argc, argv, options), vm);
        if (vm.count("help")) {
            std::cout << options << std::endl;
            std::cout << "Every option can be also set using environment variable with LOGPASS__ prefix\n" << std::endl;
            return 0;
        }

        if (vm.count("print-environment-variables")) {
            std::cout << "Available environment variables:" << std::endl;
            std::cout << std::setw(30) << std::setfill(' ') << std::left << "Environment variable" << " | ";
            std::cout << std::setw(30) << std::setfill(' ') << std::left << "Default value" << " | ";
            std::cout << std::setw(30) << std::setfill(' ') << std::left << "Description" << std::endl;
            for (auto& option : options.options()) {
                std::string key = "LOGPASS__"s + boost::algorithm::to_upper_copy(option->long_name());
                boost::algorithm::replace_all(key, "-", "_");
                std::string parameter = option->format_parameter();
                std::string description = option->description();
                if (parameter.empty()) {
                    continue;
                }
                std::smatch smatch;
                if (std::regex_search(parameter, smatch, std::regex("[^=]*=(.*)\\)"))) {
                    parameter = smatch[1];
                } else {
                    parameter = "";
                }
                if (description.size() > 0) {
                    std::transform(description.begin(), description.begin() + 1, description.begin(), ::toupper);
                }
                std::cout << std::setw(30) << std::setfill(' ') << std::left << key << " | ";
                std::cout << std::setw(30) << std::setfill(' ') << std::left << parameter << " | ";
                std::cout << std::setw(30) << std::setfill(' ') << std::left << description << std::endl;
            }
            return 0;
        }

        // parse environment variables
        program_options::store(program_options::parse_environment(options, [&options](std::string env) {
            const std::string prefix = "LOGPASS__";
            if (env.find(prefix) != 0) {
                return ""s;
            }
            env = env.substr(prefix.length());
            boost::algorithm::to_lower(env);
            boost::algorithm::replace_all(env, "_", "-");
            if (std::any_of(options.options().cbegin(), options.options().cend(),
                            [env](auto opt) { return env == opt->long_name(); })) {
                return env;
            }
            return ""s;
        }), vm);

        // parse config file
        try {
            std::string configFileName = vm["config"].as<std::string>();
            std::ifstream configFile(configFileName);
            if (!configFile.is_open() || !configFile.good()) {
                if (configFileName != "config.ini") {
                    std::cerr << "Can't open config file: " << configFileName;
                    return -1;
                }
            } else {
                program_options::store(program_options::parse_config_file(configFile, options), vm);
                std::cout << "Loaded config file " << configFileName << std::endl;
            }
        } catch (const program_options::reading_file&) {}

        // notify
        program_options::notify(vm);
    } catch (const program_options::error& e) {
        std::cerr << e.what();
        return -1;
    }

    // configure logging
    SET_THREAD_NAME("main");

    logging::core::get()->set_filter
    (
        logging::trivial::severity >= vm["log-level"].as<logging::trivial::severity_level>()
    );

    auto sink = boost::log::add_console_log(std::cout);

    namespace expr = boost::log::expressions;
    sink->set_formatter
    (
        expr::stream <<
        "[" << expr::format_date_time(expr::attr<boost::posix_time::ptime>("TimeStamp"), "%Y-%m-%d %H:%M:%S") <<
        "] [" << std::setw(8) << std::setfill('0') << expr::attr<boost::log::thread_id>("ThreadID") <<
        "] [" << std::setw(13) << std::setfill(' ') << expr::attr<std::string>("ThreadName") <<
        "] [" << std::setw(7) << std::setfill(' ') << expr::attr<boost::log::trivial::severity_level>("Severity") <<
        "] [" << expr::attr<std::string>("Class") <<
        "] [" << expr::attr<std::string>("ID") <<
        "] " << expr::message
    );

    logging::add_common_attributes();

    std::set_terminate([] {
        try {
            std::cerr << boost::stacktrace::stacktrace();
            std::cerr.flush();
            LOG(fatal) << boost::stacktrace::stacktrace();
            std::cerr.flush();
            std::clog.flush();
            std::cout.flush();
        } catch (...) {}
        std::abort();
    });

    // start filesystem
    SharedThread<Filesystem> filesystem(vm["dir"].as<std::string>());

    // load options
    DatabaseOptions databaseOptions;
    BlockchainOptions blockchainOptions;
    CommunicationOptions communicationOptions;
    ApiOptions apiOptions;

    try {
        databaseOptions = DatabaseOptions::loadOptions(vm);
        blockchainOptions = BlockchainOptions::loadOptions(vm, *filesystem);
        communicationOptions = CommunicationOptions::loadOptions(vm, *filesystem);
        apiOptions = ApiOptions::loadOptions(vm);
    } catch (const boost::program_options::error& error) {
        LOG(fatal) << "Error occurred while loading configuration: " << error.what();
        return -1;
    }

    LOG(info) << "Correctly loaded configuration";
    LOG(info) << "Public key: " << blockchainOptions.minerKey.publicKey();
    LOG(info) << "MinerId: " << MinerId(blockchainOptions.minerKey.publicKey());

    {
        // start database
        SharedThread<Database> database(databaseOptions, filesystem);
        // start blockchain
        SharedThread<Blockchain> blockchain(blockchainOptions, database);

        // start communication
        SharedThread<Communication> communication;
        if (communicationOptions.port != 0) {
            communication = SharedThread<Communication>(communicationOptions, blockchain, database);
        }

        // start API
        SharedThread<Api> api;
        if (apiOptions.port != 0) {
            api = SharedThread<Api>(apiOptions, blockchain, database, communication);
        }

        // run till SIGINT / SIGTERM
        asio::io_context context;
        asio::signal_set signals(context, SIGINT, SIGTERM);
        signals.async_wait([](auto error, auto signal) {
            if (error) {
                LOG(info) << "An error occurred while waiting for signal: " << error.message();
            } else {
                LOG(info) << "Recived signal: " << signal;
            }
        });
        context.run();

        LOG(info) << "Shutting down...";
    }

    filesystem.reset();
    LOG(info) << "Exiting...";
    return 0;
}
