/* Copyright (c) 2016-2022, The C++ IPFS client library developers

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
#include <ipfs/test/utils.h>
#include <ipfs/test/base64.hpp>

#include <iostream>
#include <stdexcept>

int main(int, char**) {
  try {
    ipfs::Client client("localhost", 5001);

    ipfs::Json dag_object = {
        {"Data", {
                {"/", {
                    {"bytes", base64::to_base64("tes")},
                }},
        }},
        {"Links", ipfs::Json::array()}
    };
    std::cout << dag_object.dump(2) << std::endl;
    std::string cid;
    client.DagPut(&dag_object, false, &cid);
    if (cid.empty()) {
        throw std::runtime_error("client.DagPut(): empty CID");
    }
    std::cout << "Got CID: " << cid << std::endl;

    ipfs::Json comp_dag_object;
    client.DagGet(cid, &comp_dag_object);
    ipfs::test::check_if_properties_exist("client.DagGet()", comp_dag_object,
                                          {"Data"});

    ipfs::Json patch = ipfs::Json::diff(dag_object, comp_dag_object);
    std::cout << patch.dump(2) << std::endl;
    if (patch.size() > 0) {
        throw std::runtime_error("client.DagGet(): DagGet returned different object than was uploaded by DagPut");
    }

    ipfs::Json resolve_result;
    client.DagResolve(cid, &resolve_result);
    std::cout << resolve_result.dump(2) << std::endl;

    ipfs::test::check_if_properties_exist("client.DagResolve()", resolve_result,
                                          {"RemPath"});

    ipfs::Json stat_result;
    client.DagStat(cid, &stat_result);
    std::cout << stat_result.dump(2) << std::endl;

    ipfs::test::check_if_properties_exist("client.DagStat()", stat_result,
                                          {"DagStats"});

    std::stringstream cab;
    client.DagExport(cid, &cab);
    std::cout << cab.str() << std::endl;

    std::string ncid;
    client.DagImport({"file", ipfs::http::FileUpload::Type::kFileContents, cab.str()}, true, &ncid);
    std::cout << ncid << std::endl;
    if (ncid.compare(cid) != 0) {
        throw std::runtime_error("client.DagImport(): returned different content from client.DagExport() CAB");
    }
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
