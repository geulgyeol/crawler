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
    CRAWLER_NAME(config["crawler_name"].as<string>()),
    USER_AGENT(config["user_agent"]["prefix"].as<string>() + CRAWLER_NAME + config["user_agent"]["suffix"].as<string>()),
    LINK_KV_ENDPOINT(config["link_kv_endpoint"].as<string>()),
    HTML_STORAGE_ENDPOINT(config["html_storage_endpoint"].as<string>()),
    PULSAR_SERVICE_URL(config["pulsar_service_url"].as<string>()),
    PULSAR_NAMESPACE(config["pulsar_namespace"].as<string>()),
    MAX_CONCURRENT_REQUESTS(config["max_concurrent_requests"].as<int>()),
    BODIES_THRESHOLD(config["bodies_threshold"].as<int>()),
    NAVER_TIMEOUT_WAITING_TIME(config["naver_timeout_waiting_time"].as<int>()),
    MAX_MESSAGE_QUEUE_SIZE(config["max_message_queue_size"].as<int>()),
    MAX_BATCHING_MESSAGE_COUNT(config["max_batching_message_count"].as<int>()),
    MAX_BATCHING_DELAY(config["max_batching_delay"].as<int>()),
    ROBOTS_CACHE_DURATION_SECONDS(config["robots_cache_duration_seconds"].as<int>()),
    MAX_ROBOTS_CACHE_SIZE(config["max_robots_cache_size"].as<int>()),
    CONNECTION_TIMEOUT_SECONDS(config["connecting_timeout_seconds"].as<int>()),
    RESPONSE_TIMEOUT_SECONDS(config["response_timeout_seconds"].as<int>()),
    VERBOSE(config["verbose"].as<bool>()),
    LOG_LEVEL(tologlevel(config["log_level"].as<string>()))
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
        CRAWL_PER_SECOND_MAP.insert({CRAWL_PER_SECOND_MAP_KEYS[i], config["crawl_per_second_map"][CRAWL_PER_SECOND_MAP_KEYS[i]].as<int>()});
    }
}
