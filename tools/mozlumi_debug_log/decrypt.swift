// Copyright 2026
// Licensed under the same terms as Mozc.
//
// Mozlumi (1.3.4): reads an exported debug log (.mozlumilog).
//
//   swift decrypt.swift <private key file> <file.mozlumilog> [output text file]
//
// Without the output file, the text is written to the standard output.

import Foundation
import Security

func fail(_ message: String) -> Never {
  FileHandle.standardError.write((message + "\n").data(using: .utf8)!)
  exit(1)
}

let arguments = CommandLine.arguments
guard arguments.count == 3 || arguments.count == 4 else {
  FileHandle.standardError.write(
    "usage: swift decrypt.swift <private key file> <file.mozlumilog> [output text file]\n"
      .data(using: .utf8)!)
  exit(2)
}

guard let privateData = FileManager.default.contents(atPath: arguments[1]) else {
  fail("cannot read \(arguments[1])")
}
let attributes: [String: Any] = [
  kSecAttrKeyType as String: kSecAttrKeyTypeECSECPrimeRandom,
  kSecAttrKeyClass as String: kSecAttrKeyClassPrivate,
  kSecAttrKeySizeInBits as String: 256,
]
var error: Unmanaged<CFError>?
guard let privateKey = SecKeyCreateWithData(privateData as CFData, attributes as CFDictionary, &error)
else {
  fail("not a private key: \(String(describing: error?.takeRetainedValue()))")
}

guard let fileData = FileManager.default.contents(atPath: arguments[2]) else {
  fail("cannot read \(arguments[2])")
}
let magic = "MOZLUMI-DEBUG-LOG 1\n".data(using: .utf8)!
guard fileData.starts(with: magic) else {
  fail("\(arguments[2]) is not a Mozlumi debug log")
}
let encrypted = fileData.subdata(in: magic.count..<fileData.count)
guard
  let plain = SecKeyCreateDecryptedData(
    privateKey, .eciesEncryptionCofactorVariableIVX963SHA256AESGCM, encrypted as CFData, &error)
    as Data?
else {
  fail("cannot decrypt (another key?): \(String(describing: error?.takeRetainedValue()))")
}

if arguments.count == 4 {
  guard FileManager.default.createFile(atPath: arguments[3], contents: plain) else {
    fail("cannot write \(arguments[3])")
  }
  print("written: \(arguments[3])")
} else {
  FileHandle.standardOutput.write(plain)
}
