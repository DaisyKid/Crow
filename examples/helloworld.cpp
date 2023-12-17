#include <iostream>
#include <fstream>
#include <vector>
#include <iterator>
#include <sstream>

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
    std::string filePath = "/home/ubuntu/Crow/updater/localVersion.txt";
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

int main()
{
    crow::SimpleApp app;

    // {ip}:8080/params?version=1.0
    CROW_ROUTE(app, "/params")
    ([](const crow::request& req) {

        std::string remoteVersion = req.url_params.get("version");
        std::string needForceUpdate = "no";
        std::string localVersion = readLocalVersion(needForceUpdate);

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
        locationElement->SetText("http://49.235.100.232:8080/setup");
        gupElement->InsertEndChild(locationElement);

        // save and convert
        XMLPrinter printer;
        doc.Print(&printer);

        return crow::response{printer.CStr()};
    });

    CROW_ROUTE(app, "/setup")
    ([](const crow::request&, crow::response& res) {

        res.set_static_file_info("setup");
        res.set_header("content-type", "application/octet-stream");
        res.end();

    });

    app.port(8080).run();
}
