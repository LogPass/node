// definitions
#ifdef _MSC_VER
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601
#endif
#define BOOST_ENABLE_ASSERT_DEBUG_HANDLER

// disable warnings for external libs
#pragma warning(push, 0)

// c libs
#include <cmath>
#include <cstdint>
#include <cstdlib>

// std libs
#include <array>
#include <charconv>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <random>
#include <ranges>
#include <regex>
#include <set>
#include <shared_mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace chrono = std::chrono;
namespace fs = std::filesystem;
namespace views = std::views;
using namespace std::string_literals;

// uwebsockets
#include <uwebsockets/App.h>

// boost
#include <boost/algorithm/string.hpp>
#include <boost/asio.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/assert.hpp>
#include <boost/bimap.hpp>
#include <boost/endian/conversion.hpp>
#include <boost/exception/exception.hpp>
#include <boost/log/core.hpp>
#include <boost/log/attributes/constant.hpp>
#include <boost/log/attributes/mutable_constant.hpp>
#include <boost/log/attributes/timer.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/attr.hpp>
#include <boost/log/expressions/attr_fwd.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/sources/basic_logger.hpp>
#include <boost/log/sources/channel_logger.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/trivial.hpp>
#include <boost/program_options.hpp>
#include <boost/stacktrace.hpp>
#include <boost/system/error_code.hpp>
#include <boost/throw_exception.hpp>

namespace asio = boost::asio;
namespace program_options = boost::program_options;
namespace logging = boost::log;

// openssl
#include <openssl/conf.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

// iroha-ed25519
#ifdef USE_IROHA_ED25519
#include <ed25519/ed25519.h>
#endif

// json
#include <nlohmann/json.hpp>
using json = nlohmann::json;

// rocksdb
#include <rocksdb/convenience.h>
#include <rocksdb/db.h>
#include <rocksdb/env.h>
#include <rocksdb/filter_policy.h>
#include <rocksdb/merge_operator.h>

// cppcodec
#include <cppcodec/base64_rfc4648.hpp>
#include <cppcodec/base64_url.hpp>
#include <cppcodec/base64_url_unpadded.hpp>

using base64 = cppcodec::base64_rfc4648;
using base64url = cppcodec::base64_url;
using base64url_unpadded = cppcodec::base64_url_unpadded;

// zlib
#include <zlib.h>

// restore warnings
#pragma warning(pop)

// definitions
#define THROW_EXCEPTION(x) BOOST_THROW_EXCEPTION(x)
#define ASSERT(x) BOOST_ASSERT(x)

// const
#include "const.h"

// tools
#include "tools/logger.hpp"
#include "tools/assert.hpp"
#include "tools/serializer.hpp"
#include "tools/endpoint.hpp"
#include "tools/enum_printer.hpp"
#include "tools/exception.hpp"
#include "tools/safe_callback.hpp"
#include "tools/json_templates.hpp"
#include "tools/performance_timer.hpp"
#include "tools/thread.hpp"
#include "tools/event_loop_thread.hpp"
#include "tools/shared_thread.hpp"

// crypto
#include "crypto/crypto.h"
