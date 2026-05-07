#include "pch.h"
#include "config.h"
#include <string>

using namespace std;

Config::Config() :
    CRAWLER_NAME("geulgyeol-crawler"),
    USER_AGENT("User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:146.0; compatible; " + CRAWLER_NAME + "/2.0; +https://github.com/geulgyeol) Gecko/20100101 Firefox/145.0"),
    LINK_KV_ENDPOINT("localhost:8082"), //link-kv.default.svc.cluster.local 
    HTML_STORAGE_ENDPOINT("localhost:8081"), //html-precompressor.default.svc.cluster.local
    PULSAR_SERVICE_URL("pulsar+ssl://pulsar.tail372bfc.ts.net:6651"), //pulsar+ssl://pulsar.tail372bfc.ts.net:6651, pulsar://localhost:6650
    PULSAR_NAMESPACE("persistent://public/default/"),
    MAX_CONCURRENT_REQUESTS(10),
    BODIES_THRESHOLD(100),
    NAVER_TIMEOUT_WAITING_TIME(60000),
    MAX_MESSAGE_QUEUE_SIZE(100),
    MAX_BATCHING_MESSAGE_COUNT(100),
    MAX_BATCHING_DELAY(1000),
    ROBOTS_CACHE_DURATION_SECONDS(3600),
    MAX_ROBOTS_CACHE_SIZE(100),
    CONNECTION_TIMEOUT_SECONDS(30L),
    RESPONSE_TIMEOUT_SECONDS(30L),
    VERBOSE(false)
{
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_N", 3 });
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_T", 25 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_N", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_T", 6 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_N", 10 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_T", 25 });
}
