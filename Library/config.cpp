#include "pch.h"
#include "config.h"
#include <string>
#include <filesystem>

using namespace std;

const pulsar::Logger::Level tologlevel(string LOG_LEVEL_STRING) {
    if (LOG_LEVEL_STRING == "INFO") return pulsar::Logger::Level::LEVEL_INFO;
    if (LOG_LEVEL_STRING == "DEBUG") return pulsar::Logger::Level::LEVEL_DEBUG;
    if (LOG_LEVEL_STRING == "WARN") return pulsar::Logger::Level::LEVEL_WARN;
    if (LOG_LEVEL_STRING == "ERROR") return pulsar::Logger::Level::LEVEL_ERROR;
}

static YAML::Node LoadConfig() {
    string path = "config.yaml";
    if (!filesystem::exists(path)) path = "../config.yaml";

    return YAML::LoadFile(path);
}

Config::Config() :
    config(LoadConfig()),
    CRAWLER_NAME(config["CRAWLER_NAME"].as<string>()),
    USER_AGENT(config["USER_AGENT"]["prefix"].as<string>() + CRAWLER_NAME + config["USER_AGENT"]["suffix"].as<string>()),
    LINK_KV_ENDPOINT(config["LINK_KV_ENDPOINT"].as<string>()),
    HTML_STORAGE_ENDPOINT(config["HTML_STORAGE_ENDPOINT"].as<string>()),
    PULSAR_SERVICE_URL(config["PULSAR_SERVICE_URL"].as<string>()),
    PULSAR_NAMESPACE(config["PULSAR_NAMESPACE"].as<string>()),
    MAX_CONCURRENT_REQUESTS(config["MAX_CONCURRENT_REQUESTS"].as<int>()),
    BODIES_THRESHOLD(config["BODIES_THRESHOLD"].as<int>()),
    NAVER_TIMEOUT_WAITING_TIME(config["NAVER_TIMEOUT_WAITING_TIME"].as<int>()),
    MAX_MESSAGE_QUEUE_SIZE(config["MAX_MESSAGE_QUEUE_SIZE"].as<int>()),
    MAX_BATCHING_MESSAGE_COUNT(config["MAX_BATCHING_MESSAGE_COUNT"].as<int>()),
    MAX_BATCHING_DELAY(config["MAX_BATCHING_DELAY"].as<int>()),
    ROBOTS_CACHE_DURATION_SECONDS(config["ROBOTS_CACHE_DURATION_SECONDS"].as<int>()),
    MAX_ROBOTS_CACHE_SIZE(config["MAX_ROBOTS_CACHE_SIZE"].as<int>()),
    CONNECTION_TIMEOUT_SECONDS(config["CONNECTION_TIMEOUT_SECONDS"].as<int>()),
    RESPONSE_TIMEOUT_SECONDS(config["RESPONSE_TIMEOUT_SECONDS"].as<int>()),
    VERBOSE(config["VERBOSE"].as<bool>()),
    LOG_LEVEL(tologlevel(config["LOG_LEVEL"].as<string>()))
{
    vector<string> CRAWL_PER_SECOND_MAP_KEYS = {
        "LinkFinder_N",
        "LinkFinder_T",
        "ProfileFinder_N",
        "ProfileFinder_T",
        "HTMLCrawler_N",
        "HTMLCrawler_T",
        "ImageDownloader_N",
        "ImageDownloader_T"
    };

    for (int i = 0; i < CRAWL_PER_SECOND_MAP_KEYS.size(); i++) {
        CRAWL_PER_SECOND_MAP.insert({CRAWL_PER_SECOND_MAP_KEYS[i], config["CRAWL_PER_SECOND_MAP"][CRAWL_PER_SECOND_MAP_KEYS[i]].as<int>()});
    }
}
