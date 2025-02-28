/* Copyright (c) 2016-2021, The C++ IPFS client library developers

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

#include <iostream>
#include <sstream>
#include <stdexcept>

int main(int, char**) {
  try {
    // Try Files API tests with time-out setting of 20 seconds
    ipfs::Client client("localhost", 5001, "20s");

    /** [ipfs::Client::FilesGet] */
    std::stringstream contents;
    client.FilesGet(
        "/ipfs/QmYwAPJzv5CZsnA625s3Xf2nemtYgPpHdWEz79ojWnPbdG/readme",
        &contents);
    std::cout << "Retrieved contents: " << contents.str().substr(0, 8) << "..."
              << std::endl;
    /* An example output:
    Retrieved contents: Hello an...
    */

    /** [ipfs::Client::FilesGet] */
    ipfs::test::check_if_string_contains("client.FilesGet()", contents.str(),
                                         "Hello and Welcome to IPFS!");

    /** [ipfs::Client::FilesAdd] */
    ipfs::Json add_result;
    client.FilesAdd(
        {{"foo.txt", ipfs::http::FileUpload::Type::kFileContents, "abcd"},
         {"bar.txt", ipfs::http::FileUpload::Type::kFileName,
          "../compile_commands.json"}},
        &add_result);
    std::cout << "FilesAdd() result:" << std::endl
              << add_result.dump(2) << std::endl;
    /* An example output:
    [
      {
        "path": "foo.txt",
        "hash": "QmWPyMW2u7J2Zyzut7TcBMT8pG6F2cB4hmZk1vBJFBt1nP",
        "size": 4
      }
      {
        "path": "bar.txt",
        "hash": "QmVjQsMgtRsRKpNM8amTCDRuUPriY8tGswsTpo137jPWwL",
        "size": 1176
      },
    ]
    */
    /** [ipfs::Client::FilesAdd] */
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
    return 1;
  }

  return 0;
}
