def is_iso_date_key:
  type == "string" and test("^[0-9]{4}-[0-9]{2}-[0-9]{2}$");

def valid_event:
  (.id | type == "string") and
  (.calendarId | type == "string") and
  (.calendarName | type == "string") and
  (.color | type == "string") and
  (.dateKey | is_iso_date_key) and
  (.start | type == "string") and
  (.end | type == "string") and
  (.allDay | type == "boolean") and
  (.title | type == "string") and
  (.location | type == "string") and
  (.eventUrl | type == "string");

if (.version == 1 and (.events | type == "array") and all(.events[]; valid_event))
then { version, eventCount: (.events | length), valid: true }
else error("calendar-events.json does not satisfy the version 1 contract")
end
