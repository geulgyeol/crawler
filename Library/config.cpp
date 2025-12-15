#include "pch.h"
#include "config.h"
#include <string>

using namespace std;

Config::Config() :
    PROJECT_ID("geulgyeoltest"),
    PROFILE_TOPIC_ID("blogProfileTopic"),
    WRITING_TOPIC_ID("blogWritingLink"),
    PROFILE_SUB_ID("blogProfileSub"),
    WRITING_FOR_PROFILE_SUB_ID("blogWritingLinkForProfileSub"),
    WRITING_FOR_CONTENT_SUB_ID("blogWritingLinkForContentSub"),
    CRAWLER_NAME("geulgyeol-crawler"),
    USER_AGENT("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:144.0; compatible; " + CRAWLER_NAME + "/2.0; +https://github.com/geulgyeol) Gecko/20100101 Firefox/144.0"),
    LINK_KV_ENDPOINT("localhost:8080"), //link-kv.default.svc.cluster.local
    HTML_STORAGE_ENDPOINT("localhost:8081"), //html-storage.default.svc.cluster.local
    MAX_CONCURRENT_REQUESTS(10),
    BODIES_THRESHOLD(100),
    DEFAULT_SUB_WAITING_TIME(5),
    ENABLE_MESSAGE_QUEUE_THRESHOLD(40),
    DISABLE_MESSAGE_QUEUE_THRESHOLD(100),
    ROBOTS_CACHE_DURATION_SECONDS(3600),
    MAX_ROBOTS_CACHE_SIZE(100),
    ENABLE_DB_UPLOAD(true)

{
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_N", 3 });
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_T", 25 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_N", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_T", 6 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_N", 15 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_T", 25 });
}
