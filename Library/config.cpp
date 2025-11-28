#include "pch.h"
#include "config.h"
#include <string>
using namespace std;

Config::Config() :
    PROJECT_ID("sejong-llms"),
    PROFILE_TOPIC_ID("s2-blogProfileTopic"),
    WRITING_TOPIC_ID("s2-blogWritingLink"),
    PROFILE_SUB_ID("s2-blogProfileSub"),
    WRITING_FOR_PROFILE_SUB_ID("s2-blogWritingLinkForProfileSub"),
    WRITING_FOR_CONTENT_SUB_ID("s2-blogWritingLinkForContentSub"),
    ORDERING_KEY("ordering_key"),
    CRAWLER_NAME("geulgyeol-crawler"),
    USER_AGENT("User-Agent: Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:144.0; compatible; " + CRAWLER_NAME + "/2.0; +https://github.com/geulgyeol) Gecko/20100101 Firefox/145.0"),
    LINK_KV_ENDPOINT("link-kv.default.svc.cluster.local"), //
    HTML_STORAGE_ENDPOINT("html-storage.default.svc.cluster.local"), //
    MAX_CONCURRENT_REQUESTS(10),
    DEFAULT_SUB_WAITING_TIME(5),
    ROBOTS_CACHE_DURATION_SECONDS(3600),
    MAX_ROBOTS_CACHE_SIZE(100),
    ENABLE_DB_UPLOAD(true)

{
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_N", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "LinkFinder_T", 20 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_N", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "ProfileFinder_T", 4 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_N", 10 });
    CRAWL_PER_SECOND_MAP.insert({ "HTMLCrawler_T", 20 });
}
