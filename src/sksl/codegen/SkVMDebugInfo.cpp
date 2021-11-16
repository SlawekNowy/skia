/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "src/core/SkStreamPriv.h"
#include "src/sksl/codegen/SkVMDebugInfo.h"
#include "src/utils/SkJSON.h"
#include "src/utils/SkJSONWriter.h"

namespace SkSL {

void SkVMDebugInfo::dump(SkWStream* o) const {
    for (size_t index = 0; index < fSlotInfo.size(); ++index) {
        const SkVMSlotInfo& info = fSlotInfo[index];

        o->writeText("$");
        o->writeDecAsText(index);
        o->writeText(" = ");
        o->writeText(info.name.c_str());
        o->writeText(" (");
        switch (info.numberKind) {
            case Type::NumberKind::kFloat:      o->writeText("float"); break;
            case Type::NumberKind::kSigned:     o->writeText("int"); break;
            case Type::NumberKind::kUnsigned:   o->writeText("uint"); break;
            case Type::NumberKind::kBoolean:    o->writeText("bool"); break;
            case Type::NumberKind::kNonnumeric: o->writeText("???"); break;
        }
        if (info.rows * info.columns > 1) {
            o->writeDecAsText(info.columns);
            if (info.rows != 1) {
                o->writeText("x");
                o->writeDecAsText(info.rows);
            }
            o->writeText(" : ");
            o->writeText("slot ");
            o->writeDecAsText(info.componentIndex + 1);
            o->writeText("/");
            o->writeDecAsText(info.rows * info.columns);
        }
        o->writeText(", L");
        o->writeDecAsText(info.line);
        o->writeText(")");
        o->newline();
    }
    o->newline();
}

void SkVMDebugInfo::writeTrace(SkWStream* w) const {
    SkJSONWriter json(w);

    json.beginObject(); // root
    json.beginArray("slots");

    for (size_t index = 0; index < fSlotInfo.size(); ++index) {
        const SkVMSlotInfo& info = fSlotInfo[index];

        json.beginObject();
        json.appendS32("slot", index);
        json.appendString("name", info.name.c_str());
        json.appendS32("columns", info.columns);
        json.appendS32("rows", info.rows);
        json.appendS32("index", info.componentIndex);
        json.appendS32("kind", (int)info.numberKind);
        json.appendS32("line", info.line);
        json.endObject();
    }

    json.endArray(); // slots
    json.endObject(); // root
    json.flush();
}

bool SkVMDebugInfo::readTrace(SkStream* r) {
    sk_sp<SkData> data = SkCopyStreamToData(r);
    skjson::DOM json(reinterpret_cast<const char*>(data->bytes()), data->size());
    const skjson::ObjectValue* root = json.root();
    if (!root) {
        return false;
    }
    const skjson::ArrayValue* slots = (*root)["slots"];
    if (!slots) {
        return false;
    }

    fSlotInfo.clear();
    for (const skjson::ObjectValue* element : *slots) {
        if (!element) {
            return false;
        }

        // Grow the slot array to hold this element. (But don't shrink it if we somehow get our
        // slots out of order!)
        const skjson::NumberValue* slot = (*element)["slot"];
        if (!slot) {
            return false;
        }
        fSlotInfo.resize(std::max(fSlotInfo.size(), (size_t)(**slot + 1)));
        SkVMSlotInfo& info = fSlotInfo[(size_t)(**slot)];

        // Populate the SlotInfo with our JSON data.
        const skjson::StringValue* name    = (*element)["name"];
        const skjson::NumberValue* columns = (*element)["columns"];
        const skjson::NumberValue* rows    = (*element)["rows"];
        const skjson::NumberValue* index   = (*element)["index"];
        const skjson::NumberValue* kind    = (*element)["kind"];
        const skjson::NumberValue* line    = (*element)["line"];
        if (!name || !columns || !rows || !index || !kind || !line) {
            return false;
        }

        info.name = name->begin();
        info.columns = **columns;
        info.rows = **rows;
        info.componentIndex = **index;
        info.numberKind = (SkSL::Type::NumberKind)(int)**kind;
        info.line = **line;
    }

    return true;
}

}  // namespace SkSL