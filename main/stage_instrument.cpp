#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <sstream>
#include <regex>

#include "crow.h"
#include "tinyxml2.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"
#include "spdlog/cfg/env.h"   // support for loading levels from the environment variable
#include "spdlog/fmt/ostr.h"  // support for user defined types

using namespace tinyxml2;

static std::string INVALID_VERSION = "-1";
static std::string DEFAULT_VERSION = "1.0";
static std::string DEFAULT_FORCED_UPDATE = "no";

// generally the version format is like "1.0" and so on
std::vector<int> splitVersion(const std::string& version)
{
    std::vector<int> result;
    std::istringstream iss(version);
    std::string number;

    while (std::getline(iss, number, '.')) {
        result.push_back(std::stoi(number));
    }

    return result;
}

int compareVersions(const std::string& version1, const std::string& version2)
{
    std::vector<int> v1 = splitVersion(version1);
    std::vector<int> v2 = splitVersion(version2);

    for (size_t i = 0; i < std::min(v1.size(), v2.size()); ++i) {
        if (v1[i] < v2[i]) {
            return -1;
        } else if (v1[i] > v2[i]) {
            return 1;
        }
    }

    if (v1.size() < v2.size()) {
        return -1;
    } else if (v1.size() > v2.size()) {
        return 1;
    }

    return 0;
}

std::string readLocalVersion(std::string& needForceUpdate)
{
    std::string filePath = "/home/root/Crow/updater/localVersion.txt";
    std::ifstream inputFile(filePath);

    if (!inputFile.is_open()) {
        CROW_LOG_ERROR << "readLocalVersion, error opening file: " << filePath;
        return INVALID_VERSION;
    }

    std::string localVersion;
    std::getline(inputFile, localVersion);
    std::getline(inputFile, needForceUpdate);

    inputFile.close();
    return localVersion;
}

bool writeLocalVersion(const std::string& localVersion, const std::string& needForceUpdate)
{
    // Open a file stream, create the file if it does not exist.
    std::string filePath = "/home/root/Crow/updater/localVersion.txt";
    std::ofstream outputFile(filePath);

    if (outputFile.is_open()) {
        outputFile << localVersion << std::endl;
        outputFile << needForceUpdate << std::endl;

        outputFile.close();

        CROW_LOG_INFO << "Successfully wrote data to the file.";
    } else {
        CROW_LOG_ERROR << "Can't open the file";
        return 1; // return error
    }
    return 0;
}

bool isValidVersion(const std::string& version)
{
    // define the pattern
    std::regex versionPattern("^\\d+\\.\\d+$");

    // match the version with the pattern
    return std::regex_match(version, versionPattern);
}

bool isValidForcedUpdate(const std::string& forcedUpdate)
{
    if ("yes" == forcedUpdate || "no" == forcedUpdate) {
        return true;
    }  
    else {
        return false;
    }
}

class CustomLogger : public crow::ILogHandler {
public:
    CustomLogger() {
        // Create a file rotating logger with 50mb size max and 10 rotated files.
        m_rotatingLogger =
            spdlog::rotating_logger_mt("stage_instrument_crow", "logs/stage_instrument_crow.log", 1048576 * 50, 10);
        spdlog::flush_every(std::chrono::seconds(1));
    }
    void log(std::string message, crow::LogLevel level) {
        if (crow::LogLevel::Critical == level) {
            m_rotatingLogger->critical(message.c_str());
        } else if (crow::LogLevel::Error == level) {
            m_rotatingLogger->error(message.c_str());
        } else if (crow::LogLevel::Warning == level) {
            m_rotatingLogger->warn(message.c_str());
        } else if (crow::LogLevel::Info == level) {
            m_rotatingLogger->info(message.c_str());
        } else if (crow::LogLevel::Debug == level) {
            m_rotatingLogger->debug(message.c_str());
        }
    }
private:
    std::shared_ptr<spdlog::logger> m_rotatingLogger;
};

int main()
{
    CustomLogger logger;
    crow::logger::setHandler(&logger);

    crow::SimpleApp app;

    //
	// Response the request info about the version
	//
    // {ip}:8080/params?version=1.0
    CROW_ROUTE(app, "/params")
    ([](const crow::request& req) {
        std::string remoteVersion = req.url_params.get("version");
        if (!isValidVersion(remoteVersion)) {
            return crow::response(400);
        }
        
        std::string needForceUpdate = DEFAULT_FORCED_UPDATE;
        std::string localVersion = readLocalVersion(needForceUpdate);

        if (!isValidVersion(localVersion)) {
            return crow::response(400);
        }

        std::string needUpdate = "no";
        if (compareVersions(localVersion, remoteVersion) == 1) {
            needUpdate = "yes";
        }

        CROW_LOG_INFO << "getResponseXml, localVersion = " << localVersion
                        << ", remoteVersion = " << remoteVersion
                        << ", needUpdate = " << needUpdate
                        << ", needForceUpdate = " << needForceUpdate;

        // create XML doc
        XMLDocument doc;

        // add the root element
        XMLElement* gupElement = doc.NewElement("GUP");
        doc.InsertEndChild(gupElement);

        // add the sub elements
        XMLElement* needUpdateElement = doc.NewElement("NeedToBeUpdated");
        needUpdateElement->SetText(needUpdate.c_str());
        gupElement->InsertEndChild(needUpdateElement);

        XMLElement* needForceUpdateElement = doc.NewElement("NeedToBeForceUpdated");
        needForceUpdateElement->SetText(needForceUpdate.c_str());
        gupElement->InsertEndChild(needForceUpdateElement);

        XMLElement* versionElement = doc.NewElement("Version");
        versionElement->SetText(localVersion.c_str());
        gupElement->InsertEndChild(versionElement);

        XMLElement* locationElement = doc.NewElement("Location");
        locationElement->SetText("http://121.40.148.40:8080/setup");
        gupElement->InsertEndChild(locationElement);

        // save and convert
        XMLPrinter printer;
        doc.Print(&printer);

        return crow::response{printer.CStr()};
    });

    //
	// Download the setup file
	//
    CROW_ROUTE(app, "/setup")
    ([](const crow::request&, crow::response& res) {
        std::string needForceUpdate = DEFAULT_FORCED_UPDATE;
        std::string localVersion = readLocalVersion(needForceUpdate);
        std::string setupFile = "StageInstrument-" + localVersion + "-x64-Setup";
        CROW_LOG_INFO << "setupFile = " << setupFile;
        res.set_static_file_info(setupFile);
        res.set_header("content-type", "application/octet-stream");
        res.end();

    });

    //
	// Upload the setup file
	//
    CROW_ROUTE(app, "/update")
      .methods(crow::HTTPMethod::Post)([&](const crow::request& req) {
        crow::multipart::message file_message(req);
        std::string localVersion = INVALID_VERSION;
        std::string needForceUpdate = DEFAULT_FORCED_UPDATE;
        for (const auto& part : file_message.part_map) {
            const auto& part_name = part.first;
            const auto& part_value = part.second;
            CROW_LOG_INFO << "Part: " << part_name << "=" << part_value.body;
            if ("Version" == part_name) {
                if (!isValidVersion(part_value.body)) {
                    return crow::response(400);
                }
                localVersion = part_value.body;
            } else if ("ForcedUpdate" == part_name) {
                if (!isValidForcedUpdate(part_value.body)) {
                    return crow::response(400);
                }
                needForceUpdate = part_value.body;
            }
        }
        writeLocalVersion(localVersion, needForceUpdate);
        return crow::response(200);
      });

    // enables all log
    // app.loglevel(crow::LogLevel::Debug);

    app.port(8080)
      .multithreaded()
      .run();
}
