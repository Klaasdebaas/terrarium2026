#ifndef TERRARIUM_SMS_MODEM_H
#define TERRARIUM_SMS_MODEM_H
#include <stdint.h>
#include <stddef.h>

// Puur: geen hardware. SerialModemIO (sms_port.h) koppelt dit aan Serial1.
class ModemIO {
 public:
  virtual ~ModemIO() {}
  virtual int available() = 0;
  virtual int read() = 0;
  virtual void write(const char* s) = 0;
  virtual void writeByte(uint8_t b) = 0;
};

enum class SmsEventType : uint8_t { Sent, Failed, Reset };
struct SmsEvent { SmsEventType type; char text[48]; };  // eerste 47 tekens van bericht of reden

class SmsModem {
 public:
  static constexpr uint8_t QUEUE_MAX = 10;
  static constexpr size_t TEXT_MAX = 161;   // 160 tekens + NUL
  static constexpr uint8_t EVENT_MAX = 8;

  explicit SmsModem(ModemIO& io);
  void begin(const char* simPin, const char* phone);
  void update(uint32_t nowMs);
  bool enqueue(const char* text);        // max 10; bij vol vervalt het oudste
  bool ready() const;                    // geregistreerd op het netwerk
  int8_t signal() const;                 // laatste AT+CSQ (0..31), -1 onbekend/99
  bool popEvent(SmsEvent& ev);

 private:
  enum class St : uint8_t { Idle, Probe, Echo, CpinQuery, CpinEnter, Cmgf, Cscs, CregCmd,
                            CregIdle, Backoff, SimLocked, Ready, Csq, CregCheck, Prompt, Body };
  enum class Res : uint8_t { None, Ok, Err };

  void feed(int c, uint32_t nowMs);
  void handleLine(const char* line);
  void startCmd(St s, const char* cmd, uint32_t timeoutMs, uint32_t nowMs);
  void startProbe(uint32_t nowMs);
  void backoff(uint32_t nowMs, uint32_t ms);
  void pushEvent(SmsEventType t, const char* text);
  void popQueue();
  void sendFail(uint32_t nowMs);
  void doReady(uint32_t nowMs);
  static bool due(uint32_t nowMs, uint32_t t) { return (int32_t)(nowMs - t) >= 0; }

  ModemIO& io_;
  char pin_[16];
  char phone_[24];
  bool begun_;
  St st_;
  Res res_;
  uint32_t cmdStartMs_, cmdTimeoutMs_;
  uint32_t lastRxMs_, untilMs_, nextCsqMs_, gateMs_;
  bool gateActive_;
  uint8_t probeTries_, attempts_;
  bool pinNeeded_, registered_, gotCmgs_, promptSeen_;
  int8_t signal_;
  char line_[128];
  uint8_t lineLen_;
  char queue_[QUEUE_MAX][TEXT_MAX];
  uint8_t qCount_;
  SmsEvent events_[EVENT_MAX];
  uint8_t evHead_, evCount_;
};

#endif  // TERRARIUM_SMS_MODEM_H
