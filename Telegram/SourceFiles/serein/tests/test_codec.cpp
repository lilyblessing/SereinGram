#include "serein/schema/gen/history/record.h"
#include "serein/schedule/model.h"
#include "base/basic_types.h"
#include "serein/tests/require.h"

#include <doctest/doctest.h>
#include <iostream>

namespace {

[[nodiscard]] Serein::History::Record Sample() {
	using namespace Serein::History;
	auto result = Record();
	result.kind = RecordKind::Edited;
	result.peerId = -1001234567890123;
	result.messageId = 42;
	result.revision = 3;
	result.date = 1759240000;
	result.text = u"hello, world"_q;
	result.entities = { TextEntity{ u"bold"_q, 0, 5, QString() } };
	result.apiLayer = 214;
	result.tlMessage = QByteArray("\x01\x02\xff", 3);
	return result;
}

[[nodiscard]] bool Rejects(const QByteArray &raw, const char *path) {
	auto error = Serein::Codec::Error();
	const auto parsed = Serein::History::ParseRecord(raw, &error);
	return !parsed && error.path == QString::fromLatin1(path);
}

} // namespace

TEST_CASE("Codec") {
	using namespace Serein::History;
	const auto raw = SerializeRecord(Sample());
	Require(raw.contains("\"version\":1"), "document version is written");
	Require(raw.contains("\"peerId\":\"-1001234567890123\""),
		"int64 fields are written as strings");
	Require(raw.contains("\"tlMessage\":\"AQL/\""), "bytes are written as base64");
	Require(raw.contains("\"kind\":\"RECORD_KIND_EDITED\""),
		"enums are written by name");
	const auto parsed = ParseRecord(raw);
	Require(parsed && *parsed == Sample(), "record survives a round trip");

	auto numeric = raw;
	numeric.replace("\"messageId\":\"42\"", "\"messageId\":42");
	Require(ParseRecord(numeric).has_value(), "int64 accepts JSON numbers");

	Require(Rejects(QByteArray(raw).replace("\"version\":1", "\"version\":2"), "version"),
		"unknown versions are rejected");
	Require(Rejects(QByteArray(raw).replace("{\"apiLayer\"", "{\"extra\":1,\"apiLayer\""), "$"),
		"unknown fields are rejected");
	Require(Rejects(QByteArray(raw).replace("RECORD_KIND_EDITED", "RECORD_KIND_UNSPECIFIED"), "kind"),
		"schema rules reject an unspecified kind");
	Require(Rejects(QByteArray(raw).replace("\"type\":\"bold\"", "\"type\":\"Bold\""), "entities[0].type"),
		"nested rules report the item path");
	Require(Rejects(QByteArray(raw).replace("\"revision\":3", "\"revision\":3.5"), "revision"),
		"fractional numbers are not 32-bit integers");
	Require(Rejects("[]", "$"), "documents must be JSON objects");
	std::cout << "PASS: Serein schema codecs" << std::endl;
}

TEST_CASE("ScheduleConfig") {
	using namespace Serein::Schedule;
	const auto peer = quint64(777000);
	auto config = DailySchedules();
	for (const auto minute : { 0, 19 * 60 + 19, 1439 }) {
		const auto saved = WithEntry(config, peer, minute, u"ping"_q);
		Require(saved.has_value(), "entry with a valid minute rejected");
		config = *saved;
		const auto raw = SerializeConfig(config);
		Require(!raw.isEmpty(), "serialized schedule config is empty");
		const auto parsed = ReadConfig(raw);
		Require(parsed.has_value(), "schedule config failed to parse back");
		Require(*parsed == config, "schedule config did not round trip");
		Require(parsed->entries.size() == 1, "round trip changed entry count");
		Require(parsed->entries[0].minuteOfDay == minute,
			"minuteOfDay corrupted in round trip");
		Require(parsed->entries[0].peer == QString::number(peer),
			"peer id corrupted in round trip");
		Require(parsed->entries[0].text == u"ping"_q,
			"text corrupted in round trip");
		Require(!parsed->entries[0].serverScheduled,
			"serverScheduled appeared from nowhere");
	}
	const auto marked = MarkSent(config, peer, u"2026-10-14"_q);
	Require(marked.has_value(), "MarkSent rejected an existing entry");
	const auto markedParsed = ReadConfig(SerializeConfig(*marked));
	Require(markedParsed.has_value() && *markedParsed == *marked
			&& markedParsed->entries[0].lastSentDate == u"2026-10-14"_q,
		"lastSentDate did not round trip");
	const auto scheduled = SetServerScheduled(*marked, peer, true);
	Require(scheduled.has_value(), "SetServerScheduled rejected an existing entry");
	const auto scheduledParsed = ReadConfig(SerializeConfig(*scheduled));
	Require(scheduledParsed.has_value() && *scheduledParsed == *scheduled
			&& scheduledParsed->entries[0].serverScheduled,
		"serverScheduled did not round trip");
	std::cout << "PASS: Serein schedule config round trip" << std::endl;
}
