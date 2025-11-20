
#include "pch.h"
#include "framework.h"
#include "google/cloud/pubsub/publisher.h"
#include "google/cloud/pubsub/subscriber.h"
#include "../Library/ignore.h"
#include <iostream>
#include <limits>
#include <string>
#include <thread>
#include <chrono>
#include <curl/curl.h>
#include <regex>
#include <vector>
#include <mutex>
#include <future>
#include <atomic>

using namespace std;
namespace pubsub = ::google::cloud::pubsub;

struct RobotsRules {
    string userAgent;
    vector<string> disallowPaths;
};

struct RobotsCacheEntry {
    map<string, RobotsRules> rules;
    chrono::steady_clock::time_point lastUpdated = std::chrono::steady_clock::now() - std::chrono::hours(100);
    bool exists = false;

    mutex mutex;

    RobotsCacheEntry(const RobotsCacheEntry&) = delete;
    RobotsCacheEntry& operator=(const RobotsCacheEntry&) = delete;
    RobotsCacheEntry() = default;
};

mutex globalRobotsCacheMapMutex;
map<string, RobotsCacheEntry> robotsCacheMap;

const long long ROBOTS_CACHE_DURATION_SECONDS = 3600;
const size_t MAX_ROBOTS_CACHE_SIZE = 100;


Secrets secrets = Secrets();

const string NID_SES = secrets.NID_SES;
const string NID_AUT = secrets.NID_AUT;

const string CRAWLER_NAME = "geulgyeol-crawler";

const string USER_AGENT = "User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:144.0; compatible; " + CRAWLER_NAME + "/2.0; +https://github.com/geulgyeol) Gecko/20100101 Firefox/144.0";


size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp);
bool IsAllowedByRobotsGeneral(const std::string& fullUrl);

void Delay(int milliseconds) {
    this_thread::sleep_for(chrono::milliseconds(milliseconds));
}




void Publish(pubsub::Publisher publisher, vector<string> contents, string orderingKey = "") try {
    vector<google::cloud::future<google::cloud::StatusOr<string>>> futures;

    for (const auto& content : contents) {
        futures.push_back(
            ((orderingKey == "") ? 
                publisher.Publish(
                    pubsub::MessageBuilder{}
                    .SetData(content)
                    .Build()
                )
                :
                publisher.Publish(
                    pubsub::MessageBuilder{}
                    .SetData(content)
                    .SetOrderingKey(orderingKey)
                    .Build()
                )
            )
        );
    }

    for (size_t i = 0; i < futures.size(); ++i) {
        auto id = futures[i].get();
        if (!id) {
            cerr << "Publish failed for \"" << contents[i] << "\": "
                << id.status() << "\n";
        }
        else {
            cout << "\"" << contents[i] << "\" published with id=" << *id << "\n";
        }
    }
}
catch (google::cloud::Status const& status) {
    cerr << "google::cloud::Status thrown: " << status << "\n";
}


vector<string> Subscribe(pubsub::Subscriber subscriber, const int messageCnt, const int waitingTime = 5) try {
    cout << "Listening for messages on subscription" << endl;

    vector<string> messages = vector<string>(messageCnt);
    std::atomic<int> cnt{ 0 };

    std::promise<void> shutdown_promise;
    auto shutdown_future = shutdown_promise.get_future();

    auto session = subscriber.Subscribe(
        [&](pubsub::Message const& m, pubsub::AckHandler h) {

            if (cnt < messageCnt) {
                int current_index = cnt.fetch_add(1);

                if (current_index < messageCnt) {
                    messages[current_index] = m.data();
                    move(h).ack();
                    cout << " # Received message: " << m.data() << " at index " << current_index << "\n";

                    if (cnt.load() == messageCnt) {
                        try {
                            shutdown_promise.set_value();
                        }
                        catch (...) {
                            
                        }
                    }
                }
                else {
                    move(h).nack();
                }
            }
            else {
                move(h).nack();
            }
        });

    cout << "Waiting for " << messageCnt << " messages or " << waitingTime << " seconds..." << endl;

    auto status = shutdown_future.wait_for(std::chrono::seconds(waitingTime));

    session.cancel();
    session.get();
    cout << "session End" << endl;

    if (status == std::future_status::timeout) {
        int received_count = cnt.load();

        if (received_count == 0) {
            cout << "Timeout: No messages received. Returning first index to '/TIMEOUTED'.\n\n";
            messages[0] = "/TIMEOUTED";
        }
        else {
            cout << "Timeout: Only " << received_count << " messages received. Returning partial result.\n";
        }
    }
    else {
        cout << "Success: All " << messageCnt << " messages received within the time limit.\n";
    }

    return messages;
}
catch (google::cloud::Status const& status) {
    cerr << "google::cloud::Status thrown: " << status << "\n";
    return vector<string>();
}



struct curl_slist* SetCURL(CURL* curl, string* readBuffer, string url, string referer = "", string range = "", bool needNID = false) {    
    if (readBuffer) {
        readBuffer->clear();
    }

    struct curl_slist* headers = NULL;

    curl_easy_reset(curl);
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, readBuffer);
    curl_easy_setopt(curl, CURLOPT_PRIVATE, readBuffer);

    headers = curl_slist_append(headers, USER_AGENT.c_str());
    headers = curl_slist_append(headers, "X-Requested-With: XMLHttpRequest");

    if (referer != "") {
        string refererHeader = "Referer: " + referer;
        headers = curl_slist_append(headers, refererHeader.c_str());
    }

    if (range != "") {
        curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    }

    if (needNID) {
        string str = "Cookie: NID_SES=" + NID_SES + "; NID_AUT=" + NID_AUT;
        headers = curl_slist_append(headers, str.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    return headers;
}



void PrintProgressBar(int current, int total) {
    int barWidth = 50;
    float progress = (float)current / total;

    cout << "[";
    int pos = barWidth * progress;
    for (int i = 0; i < barWidth; ++i) {
        if (i < pos) cout << "=";
        else if (i == pos) cout << ">";
        else cout << " ";
    }
    cout << "] " << int(progress * 100.0) << "% " << "(" << current << "/" << total << ")" << "\r";
    cout.flush();
}




size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}



std::string ExtractDomainRoot(const string fullUrl) {
    if (fullUrl.empty()) return "";

    std::string url = fullUrl;
    std::string protocol;

    std::string::size_type protocolEnd = url.find("://");
    if (protocolEnd != std::string::npos) {
        protocol = url.substr(0, protocolEnd + 3);
        url = url.substr(protocolEnd + 3);
    }
    else {
        return "";
    }

    std::string::size_type domainEnd = url.find('/');
    std::string domain = (domainEnd != std::string::npos) ? url.substr(0, domainEnd) : url;

    return protocol + domain;
}

std::string ExtractUrlPath(const std::string& fullUrl, const std::string& domainRootUrl) {
    if (fullUrl.size() <= domainRootUrl.size()) {
        return "/";
    }
    return fullUrl.substr(domainRootUrl.size());
}



size_t RobotsWriteCallback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
    size_t totalSize = size * nmemb;
    buffer->append((char*)contents, totalSize);
    return totalSize;
}


bool CheckRules(RobotsCacheEntry& entry, const std::string& userAgent, const std::string& path) {
    if (!entry.exists) return true;

    std::vector<std::string> pathsToCheck;

    if (entry.rules.count(userAgent)) {
        pathsToCheck = entry.rules.at(userAgent).disallowPaths;
    }

    if (pathsToCheck.empty() && entry.rules.count("*")) {
        pathsToCheck = entry.rules.at("*").disallowPaths;
    }

    for (const auto& disallowedPath : pathsToCheck) {
        if (path.size() >= disallowedPath.size() && path.substr(0, disallowedPath.size()) == disallowedPath) {
            return false;
        }
    }
    return true;
}


void RefreshRobotsCache(const std::string& domainRootUrl, RobotsCacheEntry& cacheEntry) {
    std::lock_guard<std::mutex> lock(cacheEntry.mutex);

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - cacheEntry.lastUpdated).count();

    if (elapsed < ROBOTS_CACHE_DURATION_SECONDS) {
        return;
    }

    cout << "Refreshing robots.txt ..\n";

    std::string robotsUrl = domainRootUrl + "/robots.txt";
    std::string robotsContent;

    CURL* curl = curl_easy_init();
    if (!curl) return;

    struct curl_slist* headers = SetCURL(curl, &robotsContent, robotsUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    long httpCode = 0;
    CURLcode res = curl_easy_perform(curl);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    curl_easy_cleanup(curl);

    cacheEntry.lastUpdated = now;
    cacheEntry.rules.clear();

    if (res != CURLE_OK || httpCode >= 400) {
        cacheEntry.exists = false;
        std::cerr << "Robots.txt fetch failed or 40x for [" << domainRootUrl << "]. Status Code: " << httpCode << std::endl;
        return;
    }

    cacheEntry.exists = true;
    std::stringstream ss(robotsContent);
    std::string line;
    std::string currentAgent = "*";

    while (std::getline(ss, line)) {
        size_t commentPos = line.find('#');
        if (commentPos != std::string::npos) line = line.substr(0, commentPos);
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);

        if (line.empty()) continue;

        if (line.substr(0, 12) == "User-agent: ") {
            currentAgent = line.substr(12);
            cacheEntry.rules[currentAgent];
        }
        else if (line.substr(0, 10) == "Disallow: ") {
            std::string disallowedPath = line.substr(10);
            if (!disallowedPath.empty()) {
                cacheEntry.rules[currentAgent].disallowPaths.push_back(disallowedPath);
            }
        }
    }
}

bool IsAllowedByRobotsGeneral(const std::string& fullUrl) {
    if (fullUrl.empty()) return true;
    
    std::string domainRootUrl = ExtractDomainRoot(fullUrl);
    std::string path = ExtractUrlPath(fullUrl, domainRootUrl);

    if (domainRootUrl.empty()) return true;

    RobotsCacheEntry* entryPtr;

    {
        std::lock_guard<std::mutex> mapLock(globalRobotsCacheMapMutex);

        if (robotsCacheMap.size() >= MAX_ROBOTS_CACHE_SIZE) {
            std::cout << "Robots cache size (" << robotsCacheMap.size() << ") exceeded limit (" << MAX_ROBOTS_CACHE_SIZE << "). Clearing all cache entries.\n";
            robotsCacheMap.clear();
        }

        entryPtr = &robotsCacheMap[domainRootUrl];
    }

    RefreshRobotsCache(domainRootUrl, *entryPtr);

    {
        std::lock_guard<std::mutex> entryLock(entryPtr->mutex);
        return CheckRules(*entryPtr, CRAWLER_NAME, path);
    }
}