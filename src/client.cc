/* Copyright (c) 2016-2023, The C++ IPFS client library developers

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#include <ipfs/client.h>
#include <ipfs/http/transport-curl.h>
#include <ipfs/http/transport.h>

#include <iostream>
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ipfs {

Client::Client(const std::string& host, long port, const std::string& timeout,
               const std::string& protocol, const std::string& apiPath,
               bool verbose)
    : url_prefix_(protocol + host + ":" + std::to_string(port) + apiPath),
      timeout_value_(timeout) {
  http_ =
      std::unique_ptr<http::TransportCurl>(new http::TransportCurl(verbose));
}

Client::Client(const Client& other)
    : url_prefix_(other.url_prefix_), timeout_value_(other.timeout_value_) {
  http_ = nullptr;
  if (other.http_) {
    http_ = other.http_->Clone();
  }
}

Client::Client(Client&& other) noexcept
    : url_prefix_(std::move(other.url_prefix_)),
      http_(std::move(other.http_)) {}

Client& Client::operator=(const Client& other) {
  if (this == &other) {
    return *this;
  }

  url_prefix_ = other.url_prefix_;
  timeout_value_ = other.timeout_value_;

  http_ = nullptr;
  if (other.http_) {
    http_ = other.http_->Clone();
  }

  return *this;
}

Client& Client::operator=(Client&& other) noexcept {
  if (this == &other) {
    return *this;
  }

  url_prefix_ = std::move(other.url_prefix_);
  timeout_value_ = std::move(other.timeout_value_);

  http_ = std::move(other.http_);

  return *this;
}

Client::~Client() = default;

void Client::Id(Json* id) { FetchAndParseJson(MakeUrl("id"), id); }

void Client::Version(Json* version) {
  FetchAndParseJson(MakeUrl("version"), version);
}

void Client::ConfigGet(const std::string& key, Json* config) {
  std::string url;

  if (key.empty()) {
    url = MakeUrl("config/show");
  } else {
    url = MakeUrl("config", {{"arg", key}});
  }

  FetchAndParseJson(url, config);

  if (!key.empty()) {
    /* Convert
    {
      "Key": "Datastore",
      "Value": { "BloomFilterSize": 0, "GCPeriod": "1h", ... }
    }

    to

    { "BloomFilterSize": 0, "GCPeriod": "1h", ... }
    */
    GetProperty(*config, "Value", 0, config);
  }
}

void Client::ConfigSet(const std::string& key, const Json& value) {
  Json unused;
  FetchAndParseJson(MakeUrl("config", {{"arg", key}, {"arg", value.dump()}}),
                    &unused);
}

void Client::ConfigReplace(const Json& config) {
  std::stringstream unused;
  http_->Fetch(MakeUrl("config/replace"),
               {{"new_config.json", http::FileUpload::Type::kFileContents,
                 config.dump()}},
               &unused);
}

void Client::DhtFindPeer(const std::string& peer_id, Json* addresses) {
  std::stringstream body;

  http_->Fetch(MakeUrl("routing/findpeer", {{"arg", peer_id}}), {}, &body);

  /* Find the addresses of the requested peer in the response. It consists
  of many lines like this:

  {..., "Responses":[{"Addrs":["...","..."],"ID":"peer_id"}], ...}

  */
  std::string line;
  while (std::getline(body, line)) {
    Json json_chunk;

    ParseJson(line, &json_chunk);

    if (json_chunk["Responses"].is_array()) {
      for (auto& r : json_chunk["Responses"]) {
        if (r["ID"] == peer_id) {
          *addresses = r["Addrs"];
          return;
        }
      }
    }
  }

  throw std::runtime_error("Could not find info for peer " + peer_id +
                           " in response: " + body.str());
}

void Client::DhtFindProvs(const std::string& hash, Json* providers) {
  std::stringstream body;

  http_->Fetch(MakeUrl("routing/findprovs", {{"arg", hash}}), {}, &body);

  /* The reply consists of multiple lines, each one of which is a JSON, for
  example:

  {"Extra":"","ID":"QmfPZcnVAEjXABiA7StETRUKkS8FzNt968Z8HynbJR7oci","Responses":null,"Type":6}
  {"Extra":"","ID":"QmfSUo8FkKDTE8T3uhXfQUiyTz7JuMrkUFpTwLM7LLidXG","Responses":null,"Type":6}
  {"Extra":"","ID":"QmWmJvCpjMuBZX4MYWupb9GB3qNYVa1igYCsAQfSHmFJde","Responses":null,"Type":0}

  we convert that into a single JSON like:

  [
    {"Extra":"","ID":"QmfPZcnVAEjXABiA7StETRUKkS8FzNt968Z8HynbJR7oci","Responses":null,"Type":6},
    {"Extra":"","ID":"QmfSUo8FkKDTE8T3uhXfQUiyTz7JuMrkUFpTwLM7LLidXG","Responses":null,"Type":6},
    {"Extra":"","ID":"QmWmJvCpjMuBZX4MYWupb9GB3qNYVa1igYCsAQfSHmFJde","Responses":null,"Type":0}
  ]
  */

  std::string line;
  while (std::getline(body, line)) {
    Json json_chunk;

    ParseJson(line, &json_chunk);

    providers->push_back(json_chunk);
  }
}

void Client::BlockGet(const std::string& block_id, std::iostream* block) {
  http_->Fetch(MakeUrl("block/get", {{"arg", block_id}}), {}, block);
}

void Client::BlockPut(const http::FileUpload& block, Json* stat) {
  FetchAndParseJson(MakeUrl("block/put"), {block}, stat);
}

void Client::BlockStat(const std::string& block_id, Json* stat) {
  FetchAndParseJson(MakeUrl("block/stat", {{"arg", block_id}}), stat);
}

void Client::FilesGet(const std::string& path, std::iostream* response) {
  http_->Fetch(MakeUrl("cat", {{"arg", path}}), {}, response);
}

void Client::FilesAdd(const std::vector<http::FileUpload>& files,
                      Json* result) {
  std::stringstream body;

  http_->Fetch(MakeUrl("add", {{"progress", "true"}}), files, &body);

  /* The reply consists of multiple lines, each one of which is a JSON, for
  example:

  {"Name":"foo.txt","Bytes":4}
  {"Name":"foo.txt","Hash":"QmWPyMW2u7J2Zyzut7TcBMT8pG6F2cB4hmZk1vBJFBt1nP"}
  {"Name":"bar.txt","Bytes":1176}
  {"Name":"bar.txt","Hash":"QmVjQsMgtRsRKpNM8amTCDRuUPriY8tGswsTpo137jPWwL"}

  we convert that into a single JSON like:

  [
    { "path": "foo.txt", "hash": "QmWP...", "size": 4 },
    { "path": "bar.txt", "hash": "QmVj...", "size": 1176 }
  ]

  and return it to the caller. */

  /* A temporary JSON object to facilitate creating the result in case the
  reply lines are out of order. This one looks like:
  {
    "foo.txt": { "path": "foo.txt", "hash": "QmWP...", "size": 4 }
    "bar.txt": { "path": "foo.txt", "hash": "QmVj...", "size": 1176 }
  }
  */
  Json temp;

  std::string line;
  for (size_t i = 1; std::getline(body, line); ++i) {
    Json json_chunk;

    ParseJson(line, &json_chunk);

    std::string path;
    GetProperty(json_chunk, "Name", i, &path);

    temp[path]["path"] = path;

    static const char* hash = "Hash";
    if (json_chunk.find(hash) != json_chunk.end()) {
      temp[path]["hash"] = json_chunk[hash];
    }

    static const char* bytes = "Bytes";
    if (json_chunk.find(bytes) != json_chunk.end()) {
      temp[path]["size"] = json_chunk[bytes];
    }
  }

  for (Json::iterator it = temp.begin(); it != temp.end(); ++it) {
    result->push_back(it.value());
  }
}

void Client::FilesLs(const std::string& path, Json* json) {
  FetchAndParseJson(MakeUrl("file/ls", {{"arg", path}}), {}, json);
}

void Client::KeyGen(const std::string& key_name, const std::string& key_type,
                    size_t key_size, std::string* generated_key) {
  Json response;

  FetchAndParseJson(MakeUrl("key/gen", {{"arg", key_name},
                                        {"type", key_type},
                                        {"size", std::to_string(key_size)}}),
                    &response);
  *generated_key = response["Id"];
}

void Client::KeyList(Json* key_list) {
  Json response;
  FetchAndParseJson(MakeUrl("key/list", {}), &response);
  *key_list = response["Keys"];
}

void Client::KeyRm(const std::string& key_name) {
  std::stringstream body;
  http_->Fetch(MakeUrl("key/rm", {{"arg", key_name}}), {}, &body);
}

void Client::KeyRename(const std::string& old_key, const std::string& new_key) {
  std::stringstream body;
  http_->Fetch(MakeUrl("key/rename", {{"arg", old_key}, {"arg", new_key}}), {},
               &body);
}

void Client::NamePublish(const std::string& object_id,
                         const std::string& key_name, const ipfs::Json& options,
                         std::string* name_id) {
  Json response;

  std::vector<std::pair<std::string, std::string>> args;
  args = {{"arg", object_id}, {"key", key_name}};
  for (auto& elt : options.items()) {
    args.push_back({elt.key(), elt.value()});
  }

  FetchAndParseJson(MakeUrl("name/publish", args), &response);

  GetProperty(response, "Name", 0, name_id);
}

void Client::NameResolve(const std::string& name_id, std::string* path_string) {
  Json response;

  FetchAndParseJson(MakeUrl("name/resolve", {{"arg", name_id}}), &response);

  GetProperty(response, "Path", 0, path_string);
}

void Client::DagExport(std::string& cid, std::iostream* output) {
  http_->Fetch(MakeUrl("dag/export", {{"arg", cid}, {"progress", "false"}}), {}, output);
}

void Client::DagImport(const http::FileUpload& data, bool pin, std::string* cid) {
  Json response;
  FetchAndParseJson(MakeUrl("dag/import", {{"pin-roots", std::to_string(pin)}}), {data}, &response);
  response.at("Root").at("Cid").at("/").get_to(*cid);
}

void Client::DagPut(Json* input, bool pin, std::string* cid) {
  Json response;
  FetchAndParseJson(MakeUrl("dag/put", {{"pin", std::to_string(pin)}}), {{"file", http::FileUpload::Type::kFileContents, input->dump()}}, &response);
  response.at("Cid").at("/").get_to(*cid);
}

void Client::DagGet(const std::string& path, Json* data) {
  FetchAndParseJson(MakeUrl("dag/get", {{"arg", path}}), data);
}

void Client::DagResolve(const std::string& path, Json* json) {
  FetchAndParseJson(MakeUrl("dag/resolve", {{"arg", path}}), json);
}

void Client::DagStat(const std::string& root_id, Json* json) {
  FetchAndParseJson(MakeUrl("dag/stat", {{"arg", root_id}, {"progress", "false"}}), json);
}

void Client::PinAdd(const std::string& object_id) {
  Json response;

  FetchAndParseJson(MakeUrl("pin/add", {{"arg", object_id}}), &response);

  Json pins_array;
  GetProperty(response, "Pins", 0, &pins_array);

  for (const std::string pin : pins_array) {
    if (pin == object_id) {
      return;
    }
  }

  throw std::runtime_error(
      "Request to pin \"" + object_id +
      "\" got a result that does not contain it as pinned: " + response.dump());
}

void Client::PinLs(Json* pinned) {
  FetchAndParseJson(MakeUrl("pin/ls"), pinned);
}

void Client::PinLs(const std::string& object_id, Json* pinned) {
  FetchAndParseJson(MakeUrl("pin/ls", {{"arg", object_id}}), pinned);
}

void Client::PinRm(const std::string& object_id, PinRmOptions options) {
  Json response;

  const std::string recursive =
      options == PinRmOptions::RECURSIVE ? "true" : "false";

  FetchAndParseJson(
      MakeUrl("pin/rm", {{"arg", object_id}, {"recursive", recursive}}),
      &response);
}

void Client::StatsBw(Json* bandwidth_info) {
  FetchAndParseJson(MakeUrl("stats/bw"), bandwidth_info);
}

void Client::StatsRepo(Json* repo_stats) {
  FetchAndParseJson(MakeUrl("stats/repo"), repo_stats);
}

void Client::SwarmAddrs(Json* addresses) {
  FetchAndParseJson(MakeUrl("swarm/addrs"), addresses);
}

void Client::SwarmConnect(const std::string& peer) {
  Json response;
  FetchAndParseJson(MakeUrl("swarm/connect", {{"arg", peer}}), &response);
}

void Client::SwarmDisconnect(const std::string& peer) {
  Json response;
  FetchAndParseJson(MakeUrl("swarm/disconnect", {{"arg", peer}}), &response);
}

void Client::SwarmPeers(Json* peers) {
  FetchAndParseJson(MakeUrl("swarm/peers"), peers);
}

void Client::Abort() { http_->StopFetch(); }
/**
 * @example threading_example.cc
 * An example of how to use IPFS Client with threads, using the Abort() and
 * Reset() methods.
 */

void Client::Reset() { http_->ResetFetch(); }

void Client::FetchAndParseJson(const std::string& url, Json* response) {
  FetchAndParseJson(url, {}, response);
}

void Client::FetchAndParseJson(const std::string& url,
                               const std::vector<http::FileUpload>& files,
                               Json* response) {
  std::stringstream body;

  http_->Fetch(url, files, &body);

  ParseJson(body.str(), response);
}

void Client::ParseJson(const std::string& input, Json* result) {
  try {
    *result = Json::parse(input);
  } catch (const std::exception& e) {
    throw std::runtime_error(std::string(e.what()) + "\nInput JSON:\n" + input);
  }
}

template <class PropertyType>
void Client::GetProperty(const Json& input, const std::string& property_name,
                         size_t line_number, PropertyType* property_value) {
  if (input.find(property_name) == input.end()) {
    throw std::runtime_error(
        std::string("Unexpected reply: valid JSON, but without the \"") +
        property_name + "\" property on line " + std::to_string(line_number) +
        ":\n" + input.dump());
  }

  *property_value = input[property_name];
}

std::string Client::MakeUrl(
    const std::string& path,
    const std::vector<std::pair<std::string, std::string>>& parameters) {
  std::string url = url_prefix_ + "/" + path +
                    "?stream-channels=true&json=true&encoding=json";
  std::vector<std::pair<std::string, std::string>> params = parameters;

  if (!timeout_value_.empty()) {
    // Set time-out at server-side
    params.push_back(std::make_pair(std::string("timeout"), timeout_value_));
  }

  for (auto& parameter : params) {
    std::string name_url_encoded;
    http_->UrlEncode(parameter.first, &name_url_encoded);

    std::string value_url_encoded;
    http_->UrlEncode(parameter.second, &value_url_encoded);

    url += "&" + name_url_encoded + "=" + value_url_encoded;
  }

  return url;
}
} /* namespace ipfs */
