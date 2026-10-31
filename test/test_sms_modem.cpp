#include "minitest.h"
#include "sms_modem.h"
#include <string>
#include <vector>

struct FakeModemIO : ModemIO {
  struct Step { std::string prefix, reply; bool used = false; };
  std::vector<Step> script;
  std::string sent;                 // alles wat het modem-object schreef
  std::vector<std::string> cmds;    // per write()-aanroep (zonder byte-writes)
  bool autoCsq = true;              // beantwoord elke AT+CSQ met 18
  bool autoCreg = true;             // beantwoord ongescripte AT+CREG? met "geregistreerd"
  std::string inbox, pending;       // pending komt pas bij release() beschikbaar
  void expect(const char* p, const char* r) { script.push_back({p, r, false}); }
  int available() override { return (int)inbox.size(); }
  int read() override { if (inbox.empty()) return -1; int c = (unsigned char)inbox[0]; inbox.erase(0, 1); return c; }
  void write(const char* s) override {
    sent += s;
    std::string str(s);
    if (str == "\r") { cmds.back() += "\r"; checkReply(cmds.back()); return; }
    cmds.push_back(str);
    if (!str.empty() && str.back() == '\r') checkReply(str);
  }
  void writeByte(uint8_t b) override { sent += (char)b; }
  void checkReply(const std::string& cmd) {
    if (autoCsq && cmd.compare(0, 6, "AT+CSQ") == 0 && !hasScript("AT+CSQ")) { pending += "+CSQ: 18,99\r\nOK\r\n"; return; }
    if (autoCreg && cmd == "AT+CREG?\r" && !hasScript("AT+CREG?")) { pending += "+CREG: 0,1\r\nOK\r\n"; return; }
    for (auto& st : script)
      if (!st.used && cmd.compare(0, st.prefix.size(), st.prefix) == 0) { pending += st.reply; st.used = true; return; }
  }
  bool hasScript(const char* p) { for (auto& st : script) if (!st.used && st.prefix.compare(0, strlen(p), p) == 0) return true; return false; }
  void release() { inbox += pending; pending.clear(); }
  void inject(const char* s) { pending += s; }   // ongevraagd antwoord
  int count(const char* cmd) const { int n = 0; for (auto& c : cmds) if (c == cmd) n++; return n; }
};

static const char* PHONE = "+31600000000";

static void initScript(FakeModemIO& f, const char* creg = "\r\n+CREG: 0,1\r\n\r\nOK\r\n") {
  f.expect("AT\r", "OK\r\n");
  f.expect("ATE0\r", "OK\r\n");
  f.expect("AT+CPIN?\r", "\r\n+CPIN: READY\r\n\r\nOK\r\n");
  f.expect("AT+CMGF=1\r", "OK\r\n");
  f.expect("AT+CSCS=\"GSM\"\r", "OK\r\n");
  f.expect("AT+CREG?\r", creg);
}

struct Rig {
  FakeModemIO f; SmsModem m; uint32_t now = 1000;
  Rig() : m(f) {}
  void step() { f.release(); m.update(now); }
  void run(uint32_t ms, uint32_t stepMs = 100) {
    uint32_t end = now + ms;
    while ((uint32_t)(end - now) > 0 && (int32_t)(end - now) > 0) { now += stepMs; step(); }
  }
};

TEST(init_to_ready) {
  Rig r; initScript(r.f);
  r.m.begin("", PHONE);
  CHECK(!r.m.ready());
  r.run(1000);
  CHECK(r.m.ready());
  CHECK_EQ(r.f.cmds[0].c_str()[0], 'A');
  CHECK_STR(r.f.cmds[0].c_str(), "AT\r");
  CHECK_STR(r.f.cmds[1].c_str(), "ATE0\r");
  CHECK_STR(r.f.cmds[2].c_str(), "AT+CPIN?\r");
  CHECK_STR(r.f.cmds[5].c_str(), "AT+CREG?\r");
}

TEST(probe_retries_then_backoff) {
  Rig r;   // modem antwoordt nooit
  r.m.begin("", PHONE);
  r.run(10500);
  CHECK_EQ(r.f.count("AT\r"), 10);
  r.run(25000);                       // 30 s wachten, niet spinnen
  CHECK_EQ(r.f.count("AT\r"), 10);
  r.run(10000);
  CHECK(r.f.count("AT\r") > 10);
}

TEST(creg_polled_every_2s_until_registered) {
  Rig r; initScript(r.f, "\r\n+CREG: 0,2\r\n\r\nOK\r\n");
  r.m.begin("", PHONE);
  r.run(1000);
  CHECK(!r.m.ready());
  r.f.expect("AT+CREG?\r", "+CREG: 0,5\r\nOK\r\n");
  r.run(3000);
  CHECK(r.m.ready());
  CHECK_EQ(r.f.count("AT+CREG?\r"), 3);   // 2 tot registratie + 1 controle na de eerste CSQ
}

TEST(init_with_pin) {
  Rig r;
  r.f.expect("AT\r", "OK\r\n"); r.f.expect("ATE0\r", "OK\r\n");
  r.f.expect("AT+CPIN?\r", "+CPIN: SIM PIN\r\n\r\nOK\r\n");
  r.f.expect("AT+CPIN=1234\r", "OK\r\n");
  r.f.expect("AT+CMGF=1\r", "OK\r\n"); r.f.expect("AT+CSCS=\"GSM\"\r", "OK\r\n");
  r.f.expect("AT+CREG?\r", "+CREG: 0,1\r\nOK\r\n");
  r.m.begin("1234", PHONE);
  r.run(1500);
  CHECK_EQ(r.f.count("AT+CPIN=1234\r"), 1);
  CHECK(r.m.ready());
}

TEST(sim_pin_without_pin_retries_every_60s) {
  Rig r;
  for (int i = 0; i < 2; i++) {
    r.f.expect("AT\r", "OK\r\n"); r.f.expect("ATE0\r", "OK\r\n");
    r.f.expect("AT+CPIN?\r", "+CPIN: SIM PIN\r\nOK\r\n");
  }
  r.m.begin("", PHONE);
  r.run(30000);
  CHECK(!r.m.ready());
  CHECK_EQ(r.f.count("AT\r"), 1);
  r.run(40000);
  CHECK_EQ(r.f.count("AT\r"), 2);
  CHECK_EQ(r.f.count("AT+CPIN=\r"), 0);
}

TEST(wrong_pin_not_retried) {
  Rig r;
  r.f.expect("AT\r", "OK\r\n"); r.f.expect("ATE0\r", "OK\r\n");
  r.f.expect("AT+CPIN?\r", "+CPIN: SIM PIN\r\nOK\r\n");
  r.f.expect("AT+CPIN=1234\r", "+CME ERROR: incorrect password\r\n");
  r.m.begin("1234", PHONE);
  r.run(300000);
  CHECK_EQ(r.f.count("AT+CPIN=1234\r"), 1);
  CHECK(!r.m.ready());
}

TEST(signal_parsed) {
  Rig r; initScript(r.f); r.f.autoCsq = false;
  r.f.expect("AT+CSQ\r", "+CSQ: 18,99\r\nOK\r\n");
  r.m.begin("", PHONE);
  CHECK_EQ(r.m.signal(), -1);
  r.run(1500);
  CHECK(r.m.ready());
  CHECK_EQ(r.m.signal(), 18);
  r.f.expect("AT+CSQ\r", "+CSQ: 99,99\r\nOK\r\n");
  r.run(61000);
  CHECK_EQ(r.m.signal(), -1);
  CHECK_EQ(r.f.count("AT+CSQ\r"), 2);
}

static void sendOkScript(FakeModemIO& f) {
  f.expect("AT+CMGS=\"+31600000000\"\r", "\r\n> ");
}

TEST(send_sms_ok) {
  Rig r; initScript(r.f);
  r.m.begin("", PHONE);
  r.run(1500);
  CHECK(r.m.enqueue("test"));
  sendOkScript(r.f);
  size_t before = r.f.sent.size();
  r.run(300);
  CHECK(r.f.sent.substr(before).find("AT+CMGS=\"+31600000000\"\r") != std::string::npos);
  CHECK(r.f.sent.find(std::string("test") + (char)0x1A) != std::string::npos);
  SmsEvent ev;
  CHECK(!r.m.popEvent(ev));
  r.f.inject("\r\n+CMGS: 5\r\n\r\nOK\r\n");
  r.run(300);
  CHECK(r.m.popEvent(ev));
  CHECK(ev.type == SmsEventType::Sent);
  CHECK_STR(ev.text, "test");
  CHECK(!r.m.popEvent(ev));
  r.run(10000);                                   // niet nog eens versturen
  CHECK_EQ(r.f.count("AT+CMGS=\"+31600000000\"\r"), 1);
}

TEST(send_cms_error_counts_as_failure) {
  Rig r; initScript(r.f);
  r.m.begin("", PHONE);
  r.run(1500);
  r.m.enqueue("x");
  sendOkScript(r.f);
  r.run(300);
  r.f.inject("+CMS ERROR: 330\r\n");
  r.run(1000);
  SmsEvent ev;
  CHECK(!r.m.popEvent(ev));
  CHECK(r.f.sent.find((char)0x1B) != std::string::npos);
  r.run(61000);
  CHECK_EQ(r.f.count("AT+CMGS=\"+31600000000\"\r"), 2);
}

TEST(send_retry_then_fail) {
  Rig r; initScript(r.f);
  r.m.begin("", PHONE);
  r.run(1500);
  r.m.enqueue("alarm tekst");
  // modem antwoordt nooit met '>'
  const char* cmgs = "AT+CMGS=\"+31600000000\"\r";
  r.run(500);
  CHECK_EQ(r.f.count(cmgs), 1);
  r.run(10500);                                   // prompt-time-out (10 s)
  CHECK(r.f.sent.find((char)0x1B) != std::string::npos);
  r.run(55000);
  CHECK_EQ(r.f.count(cmgs), 1);                   // nog niet, pas 60 s na mislukking
  r.run(6000);
  CHECK_EQ(r.f.count(cmgs), 2);
  r.run(10500 + 290000);                          // fail + 300 s
  CHECK_EQ(r.f.count(cmgs), 2);
  r.run(15000);
  CHECK_EQ(r.f.count(cmgs), 3);
  r.run(10500 + 890000);
  CHECK_EQ(r.f.count(cmgs), 3);
  SmsEvent ev;
  CHECK(!r.m.popEvent(ev) || ev.type != SmsEventType::Failed);
  r.run(20000);
  CHECK_EQ(r.f.count(cmgs), 4);
  r.run(11000);                                   // 4e poging mislukt
  bool failed = false;
  while (r.m.popEvent(ev)) if (ev.type == SmsEventType::Failed) { failed = true; CHECK_STR(ev.text, "alarm tekst"); }
  CHECK(failed);
  r.run(2000000);                                 // wachtrij leeg: niets meer
  CHECK_EQ(r.f.count(cmgs), 4);
}

TEST(queue_overflow) {
  Rig r;
  r.m.begin("", PHONE);
  for (int i = 0; i < 11; i++) { char b[16]; snprintf(b, sizeof b, "m%d", i); r.m.enqueue(b); }
  // verifieer via versturen: alleen m1..m10 komen eruit, m0 niet
  initScript(r.f);
  for (int i = 0; i < 10; i++) r.f.expect("AT+CMGS=", "\r\n> ");
  r.run(1500);
  for (int i = 0; i < 10; i++) {
    r.run(300);
    r.f.inject("+CMGS: 1\r\nOK\r\n");
    r.run(3000);
  }
  std::vector<std::string> texts; SmsEvent ev;
  while (r.m.popEvent(ev)) texts.push_back(ev.text);
  CHECK_EQ(texts.size(), (size_t)8);   // eventwachtrij houdt de laatste 8
  CHECK_STR(texts.front().c_str(), "m3");
  CHECK_STR(texts.back().c_str(), "m10");
  CHECK(r.f.sent.find("m0") == std::string::npos);
  CHECK(r.f.sent.find("m1\x1A") != std::string::npos);
  r.run(100000);
  CHECK_EQ(r.f.count("AT+CMGS=\"+31600000000\"\r"), 10);
}

TEST(silent_modem_reset) {
  Rig r; initScript(r.f); r.f.autoCsq = false; r.f.autoCreg = false;
  r.m.begin("", PHONE);
  r.run(1500);
  CHECK(r.m.ready());
  int atBefore = r.f.count("AT\r");
  r.run(290000);
  CHECK(r.m.ready());
  SmsEvent ev;
  CHECK(!r.m.popEvent(ev));
  r.run(15000);
  CHECK(r.m.popEvent(ev));
  CHECK(ev.type == SmsEventType::Reset);
  CHECK(r.f.count("AT\r") > atBefore);
  CHECK(!r.m.ready());
}

TEST(millis_wrap) {
  Rig r; r.now = 0xFFFFFF00u - 1500; initScript(r.f);
  r.m.begin("", PHONE);
  r.run(1000);
  CHECK(r.m.ready());
  r.m.enqueue("wrap");
  r.f.expect("AT+CMGS=", "\r\n> ");
  r.run(1000);
  r.f.inject("+CMGS: 1\r\nOK\r\n");
  r.run(2000);
  SmsEvent ev;
  CHECK(r.m.popEvent(ev));
  CHECK(ev.type == SmsEventType::Sent);
}

TEST(creg_polled_with_csq_in_ready) {
  Rig r; initScript(r.f);
  r.m.begin("", PHONE);
  r.run(1500);
  CHECK(r.m.ready());
  int cregBefore = r.f.count("AT+CREG?\r");
  CHECK(cregBefore >= 2);             // init + controle na de eerste CSQ
  // na 60 s: CSQ en CREG; netwerk kwijt -> niet meer ready
  r.f.expect("AT+CREG?\r", "+CREG: 0,2\r\nOK\r\n");
  r.f.expect("AT+CREG?\r", "+CREG: 0,3\r\nOK\r\n");
  r.run(60500);
  CHECK_EQ(r.f.count("AT+CREG?\r"), cregBefore + 1);
  CHECK(!r.m.ready());
  // wachtrij blijft bewaard terwijl er opnieuw op registratie gewacht wordt (elke 2 s)
  CHECK(r.m.enqueue("na herregistratie"));
  r.run(2500);
  CHECK_EQ(r.f.count("AT+CREG?\r"), cregBefore + 2);
  CHECK(!r.m.ready());
  CHECK_EQ(r.f.count("AT+CMGS=\"+31600000000\"\r"), 0);
  // weer geregistreerd (autoCreg): ready, bericht wordt alsnog verstuurd
  sendOkScript(r.f);
  r.run(2500);
  CHECK(r.m.ready());
  CHECK_EQ(r.f.count("AT+CMGS=\"+31600000000\"\r"), 1);
}

TEST(creg_roaming_stays_ready) {
  Rig r; initScript(r.f);
  r.m.begin("", PHONE);
  r.run(1500);
  r.f.expect("AT+CREG?\r", "+CREG: 0,5\r\nOK\r\n");
  r.run(61000);
  CHECK(r.m.ready());
}
