#include "sms_modem.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const uint32_t PROBE_TIMEOUT_MS = 1000;
static const uint32_t CMD_TIMEOUT_MS = 5000;
static const uint32_t PROMPT_TIMEOUT_MS = 10000;
static const uint32_t CMGS_TIMEOUT_MS = 60000;
static const uint32_t PROBE_BACKOFF_MS = 30000;
static const uint32_t SIM_RETRY_MS = 60000;
static const uint32_t CREG_POLL_MS = 2000;
static const uint32_t CSQ_INTERVAL_MS = 60000;
static const uint32_t SILENCE_MS = 300000;
static const uint32_t NEXT_MSG_GAP_MS = 2000;
static const uint8_t PROBE_MAX = 10;
static const uint8_t RETRY_COUNT = 3;
static const uint32_t RETRY_DELAY_MS[RETRY_COUNT] = {60000UL, 300000UL, 900000UL};

static void copyTrunc(char* dst, size_t dstSize, const char* src) {
  size_t n = strlen(src);
  if (n >= dstSize) n = dstSize - 1;
  memcpy(dst, src, n);
  dst[n] = 0;
}

static bool startsWith(const char* s, const char* p) { return strncmp(s, p, strlen(p)) == 0; }

SmsModem::SmsModem(ModemIO& io)
    : io_(io), begun_(false), st_(St::Idle), res_(Res::None), cmdStartMs_(0), cmdTimeoutMs_(0),
      lastRxMs_(0), untilMs_(0), nextCsqMs_(0), gateMs_(0), gateActive_(false), probeTries_(0),
      attempts_(0), pinNeeded_(false), registered_(false), gotCmgs_(false), promptSeen_(false),
      signal_(-1), lineLen_(0), qCount_(0), evHead_(0), evCount_(0) {
  pin_[0] = 0; phone_[0] = 0; line_[0] = 0;
  memset(queue_, 0, sizeof queue_);
  memset(events_, 0, sizeof events_);
}

void SmsModem::begin(const char* simPin, const char* phone) {
  copyTrunc(pin_, sizeof pin_, simPin ? simPin : "");
  copyTrunc(phone_, sizeof phone_, phone ? phone : "");
  begun_ = true;
  st_ = St::Idle;        // update() start de initialisatie
  signal_ = -1;
  lineLen_ = 0;
  attempts_ = 0;
  gateActive_ = false;
}

bool SmsModem::ready() const {
  return st_ == St::Ready || st_ == St::Csq || st_ == St::Prompt || st_ == St::Body;
}

int8_t SmsModem::signal() const { return signal_; }

bool SmsModem::enqueue(const char* text) {
  if (!text) return false;
  if (qCount_ >= QUEUE_MAX) {
    // oudste vervalt; een bericht dat nu verstuurd wordt blijft staan
    bool busy = (st_ == St::Prompt || st_ == St::Body);
    uint8_t drop = busy ? 1 : 0;
    for (uint8_t i = drop; i + 1 < qCount_; i++) memcpy(queue_[i], queue_[i + 1], TEXT_MAX);
    qCount_--;
    if (!busy) attempts_ = 0;
  }
  copyTrunc(queue_[qCount_], TEXT_MAX, text);
  qCount_++;
  return true;
}

bool SmsModem::popEvent(SmsEvent& ev) {
  if (evCount_ == 0) return false;
  ev = events_[evHead_];
  evHead_ = (uint8_t)((evHead_ + 1) % EVENT_MAX);
  evCount_--;
  return true;
}

void SmsModem::pushEvent(SmsEventType t, const char* text) {
  if (evCount_ >= EVENT_MAX) {  // oudste weg
    evHead_ = (uint8_t)((evHead_ + 1) % EVENT_MAX);
    evCount_--;
  }
  SmsEvent& e = events_[(evHead_ + evCount_) % EVENT_MAX];
  e.type = t;
  copyTrunc(e.text, sizeof e.text, text);
  evCount_++;
}

void SmsModem::popQueue() {
  if (qCount_ == 0) return;
  for (uint8_t i = 0; i + 1 < qCount_; i++) memcpy(queue_[i], queue_[i + 1], TEXT_MAX);
  qCount_--;
}

void SmsModem::startCmd(St s, const char* cmd, uint32_t timeoutMs, uint32_t nowMs) {
  st_ = s;
  res_ = Res::None;
  cmdStartMs_ = nowMs;
  cmdTimeoutMs_ = timeoutMs;
  gotCmgs_ = false;
  promptSeen_ = false;
  io_.write(cmd);
  io_.write("\r");
}

void SmsModem::startProbe(uint32_t nowMs) {
  probeTries_ = 0;
  registered_ = false;
  pinNeeded_ = false;
  lastRxMs_ = nowMs;
  startCmd(St::Probe, "AT", PROBE_TIMEOUT_MS, nowMs);
}

void SmsModem::backoff(uint32_t nowMs, uint32_t ms) {
  st_ = St::Backoff;
  untilMs_ = nowMs + ms;
}

void SmsModem::feed(int c, uint32_t) {
  if (c == '\r' || (c == ' ' && lineLen_ == 0)) return;  // leading spaces (na prompt "> ") negeren
  if (c == '>' && lineLen_ == 0) { promptSeen_ = true; return; }  // prompt "> " heeft geen \n
  if (c == '\n') {
    line_[lineLen_] = 0;
    if (lineLen_ > 0) handleLine(line_);
    lineLen_ = 0;
    return;
  }
  if (lineLen_ < sizeof line_ - 1) line_[lineLen_++] = (char)c;  // te lange regel: rest vervalt
}

void SmsModem::handleLine(const char* l) {
  if (strcmp(l, "OK") == 0) { res_ = Res::Ok; return; }
  if (strcmp(l, "ERROR") == 0 || startsWith(l, "+CME ERROR") || startsWith(l, "+CMS ERROR")) {
    res_ = Res::Err;
    return;
  }
  if (startsWith(l, "+CPIN:")) {
    if (strstr(l, "SIM PIN") && !strstr(l, "SIM PIN2")) pinNeeded_ = true;
    return;
  }
  if (startsWith(l, "+CREG:") || startsWith(l, "+CEREG:")) {
    const char* comma = strchr(l, ',');
    if (comma) {
      int stat = atoi(comma + 1);   // "+CREG: n,stat" (bij URC zonder n: eerste getal)
      if (stat == 1 || stat == 5) registered_ = true;
    }
    return;
  }
  if (startsWith(l, "+CSQ:")) {
    int v = atoi(l + 5);
    signal_ = (v >= 0 && v <= 31) ? (int8_t)v : (int8_t)-1;
    return;
  }
  if (startsWith(l, "+CMGS:")) { gotCmgs_ = true; return; }
}

void SmsModem::sendFail(uint32_t nowMs) {
  io_.writeByte(0x1B);  // ESC: eventueel openstaande prompt annuleren
  attempts_++;
  if (attempts_ > RETRY_COUNT) {
    pushEvent(SmsEventType::Failed, qCount_ ? queue_[0] : "");
    popQueue();
    attempts_ = 0;
    gateMs_ = nowMs + NEXT_MSG_GAP_MS;
  } else {
    gateMs_ = nowMs + RETRY_DELAY_MS[attempts_ - 1];
  }
  gateActive_ = true;
  st_ = St::Ready;
}

void SmsModem::doReady(uint32_t nowMs) {
  if (gateActive_ && due(nowMs, gateMs_)) gateActive_ = false;
  if (qCount_ > 0 && !gateActive_) {
    char cmd[48];
    snprintf(cmd, sizeof cmd, "AT+CMGS=\"%s\"", phone_);
    startCmd(St::Prompt, cmd, PROMPT_TIMEOUT_MS, nowMs);
    return;
  }
  if (due(nowMs, nextCsqMs_)) {
    nextCsqMs_ = nowMs + CSQ_INTERVAL_MS;
    startCmd(St::Csq, "AT+CSQ", CMD_TIMEOUT_MS, nowMs);
  }
}

void SmsModem::update(uint32_t nowMs) {
  if (!begun_) return;
  int c;
  while (io_.available() > 0 && (c = io_.read()) >= 0) {
    lastRxMs_ = nowMs;
    feed(c, nowMs);
  }
  if (st_ == St::Idle) { startProbe(nowMs); return; }

  if (ready() && (uint32_t)(nowMs - lastRxMs_) >= SILENCE_MS) {
    pushEvent(SmsEventType::Reset, "geen antwoord van modem");
    startProbe(nowMs);
    return;
  }

  bool timedOut = (uint32_t)(nowMs - cmdStartMs_) >= cmdTimeoutMs_;
  bool ok = res_ == Res::Ok;
  bool fail = res_ == Res::Err || timedOut;

  switch (st_) {
    case St::Probe:
      if (ok) { startCmd(St::Echo, "ATE0", CMD_TIMEOUT_MS, nowMs); }
      else if (fail) {
        if (++probeTries_ >= PROBE_MAX) backoff(nowMs, PROBE_BACKOFF_MS);
        else startCmd(St::Probe, "AT", PROBE_TIMEOUT_MS, nowMs);
      }
      break;
    case St::Echo:
      if (ok) startCmd(St::CpinQuery, "AT+CPIN?", CMD_TIMEOUT_MS, nowMs);
      else if (fail) backoff(nowMs, PROBE_BACKOFF_MS);
      break;
    case St::CpinQuery:
      if (ok) {
        if (!pinNeeded_) startCmd(St::Cmgf, "AT+CMGF=1", CMD_TIMEOUT_MS, nowMs);
        else if (pin_[0] == 0) backoff(nowMs, SIM_RETRY_MS);  // SIM-fout: elke 60 s opnieuw
        else {
          char cmd[32];
          snprintf(cmd, sizeof cmd, "AT+CPIN=%s", pin_);
          startCmd(St::CpinEnter, cmd, CMD_TIMEOUT_MS, nowMs);
        }
      } else if (fail) backoff(nowMs, SIM_RETRY_MS);
      break;
    case St::CpinEnter:
      if (ok) startCmd(St::Cmgf, "AT+CMGF=1", CMD_TIMEOUT_MS, nowMs);
      else if (fail) st_ = St::SimLocked;  // verkeerde pin: niet herhalen (SIM-blokkade)
      break;
    case St::Cmgf:
      if (ok) startCmd(St::Cscs, "AT+CSCS=\"GSM\"", CMD_TIMEOUT_MS, nowMs);
      else if (fail) backoff(nowMs, PROBE_BACKOFF_MS);
      break;
    case St::Cscs:
      if (ok) { registered_ = false; startCmd(St::CregCmd, "AT+CREG?", CMD_TIMEOUT_MS, nowMs); }
      else if (fail) backoff(nowMs, PROBE_BACKOFF_MS);
      break;
    case St::CregCmd:
      if (ok) {
        if (registered_) {
          st_ = St::Ready;
          lastRxMs_ = nowMs;
          nextCsqMs_ = nowMs;
        } else {
          st_ = St::CregIdle;
          untilMs_ = nowMs + CREG_POLL_MS;
        }
      } else if (fail) backoff(nowMs, PROBE_BACKOFF_MS);
      break;
    case St::CregIdle:
      if (due(nowMs, untilMs_)) { registered_ = false; startCmd(St::CregCmd, "AT+CREG?", CMD_TIMEOUT_MS, nowMs); }
      break;
    case St::Backoff:
      if (due(nowMs, untilMs_)) startProbe(nowMs);
      break;
    case St::SimLocked:
      break;
    case St::Ready:
      doReady(nowMs);
      break;
    case St::Csq:
      if (ok || fail) { st_ = St::Ready; doReady(nowMs); }
      break;
    case St::Prompt:
      if (promptSeen_) {
        io_.write(queue_[0]);
        io_.writeByte(0x1A);
        st_ = St::Body;
        res_ = Res::None;
        gotCmgs_ = false;
        cmdStartMs_ = nowMs;
        cmdTimeoutMs_ = CMGS_TIMEOUT_MS;
      } else if (fail) sendFail(nowMs);
      break;
    case St::Body:
      if (ok && gotCmgs_) {
        pushEvent(SmsEventType::Sent, queue_[0]);
        popQueue();
        attempts_ = 0;
        gateMs_ = nowMs + NEXT_MSG_GAP_MS;
        gateActive_ = true;
        st_ = St::Ready;
      } else if (fail || ok) sendFail(nowMs);
      break;
    case St::Idle:
      break;
  }
}
