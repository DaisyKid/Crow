#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <sstream>
#include <regex>

#include "crow.h"
#include "tinyxml2.h"

using namespace tinyxml2;

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
        CROW_LOG_INFO << "readLocalVersion, error opening file: " << filePath;
        return "-1";
    }

    std::string localVersion;
    std::getline(inputFile, localVersion);
    std::getline(inputFile, needForceUpdate);

    inputFile.close();
    return localVersion;
}

void parseVersionInfo(const std::string& filename, std::string& localVersion, std::string& needForceUpdate)
{
    std::regex pattern("setup_(\\d+\\.\\d+)_([a-zA-Z]+)\\.exe");
    std::smatch matches;

    if (std::regex_search(filename, matches, pattern)) {
        localVersion = matches[1].str();
        if ("yes" == matches[2].str() || "no" == matches[2].str()) {
            needForceUpdate = matches[2].str();
        }
        CROW_LOG_INFO << "parseVersionInfo, localVersion = " << localVersion
            << ", needForceUpdate = " << needForceUpdate;
    } else {
        CROW_LOG_ERROR << "No matched version info found";
    }
}

int main()
{
    crow::SimpleApp app;

    std::string localVersion = "-1";
    std::string needForceUpdate = "no";

    //
	// Response the request info about the version
	//
    // {ip}:8080/params?version=1.0
    CROW_ROUTE(app, "/params")
    ([&](const crow::request& req) {

        std::string remoteVersion = req.url_params.get("version");
        // std::string needForceUpdate = "no";
        // std::string localVersion = readLocalVersion(needForceUpdate);

        std::string needUpdate = "no";
        if (localVersion != "-1" && compareVersions(localVersion, remoteVersion) == 1) {
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

        res.set_static_file_info("setup");
        res.set_header("content-type", "application/octet-stream");
        res.end();

    });

    //
	// Upload the setup file
	//
    CROW_ROUTE(app, "/uploadfile")
      .methods(crow::HTTPMethod::Post)([&](const crow::request& req) {
          crow::multipart::message file_message(req);
          for (const auto& part : file_message.part_map)
          {
              const auto& part_name = part.first;
              const auto& part_value = part.second;
              CROW_LOG_DEBUG << "Part: " << part_name;
              if ("InputFile" == part_name)
              {
                  // Extract the file name
                  auto headers_it = part_value.headers.find("Content-Disposition");
                  if (headers_it == part_value.headers.end())
                  {
                      CROW_LOG_ERROR << "No Content-Disposition found";
                      return crow::response(400);
                  }
                  auto params_it = headers_it->second.params.find("filename");
                  if (params_it == headers_it->second.params.end())
                  {
                      CROW_LOG_ERROR << "Part with name \"InputFile\" should have a file";
                      return crow::response(400);
                  }
                  const std::string outfile_name = params_it->second;

                  parseVersionInfo(outfile_name, localVersion, needForceUpdate);
                  CROW_LOG_INFO << "outfile_name = " << outfile_name
                    << ", localVersion = " << localVersion
                    << ", needForceUpdate = " << needForceUpdate;

                  for (const auto& part_header : part_value.headers)
                  {
                      const auto& part_header_name = part_header.first;
                      const auto& part_header_val = part_header.second;
                      CROW_LOG_DEBUG << "Header: " << part_header_name << '=' << part_header_val.value;
                      for (const auto& param : part_header_val.params)
                      {
                          const auto& param_key = param.first;
                          const auto& param_val = param.second;
                          CROW_LOG_DEBUG << " Param: " << param_key << ',' << param_val;
                      }
                  }

                  // Create a new file with the extracted file name and write file contents to it
                  std::ofstream out_file("/home/root/Crow/updater/" + outfile_name);
                  if (!out_file)
                  {
                      CROW_LOG_ERROR << " Write to file failed\n";
                      continue;
                  }
                  out_file << part_value.body;
                  out_file.close();
                  CROW_LOG_INFO << " Contents written to /home/root/Crow/updater/" << outfile_name << '\n';
              }
              else
              {
                  CROW_LOG_DEBUG << " Value: " << part_value.body << '\n';
              }
          }
          return crow::response(200);
      });

    // enables all log
    app.loglevel(crow::LogLevel::Debug);

    app.port(8080)
      .multithreaded()
      .run();
}
