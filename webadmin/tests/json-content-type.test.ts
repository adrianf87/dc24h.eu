import assert from "node:assert/strict";
import { test } from "node:test";
import { isJsonContentType } from "../server/json-content-type.ts";

test("private API adapters accept only one JSON media type with optional UTF-8", () => {
  for (const valid of ["application/json", "Application/JSON", "application/json; charset=utf-8",
    'application/json; charset="UTF-8"', "application/json ; charset = UTF-8 "]) {
    assert.equal(isJsonContentType(valid), true, valid);
  }
  for (const invalid of [undefined, null, [], "application/json-attacker", "application/json, text/plain",
    "application/json; charset=latin1", "text/json", "application/json; boundary=x", "application/json\r\nX-Evil:1"]) {
    assert.equal(isJsonContentType(invalid), false, String(invalid));
  }
});
