#ifdef ESP8266

// The certificate is stored in PMEM - 321
static const uint8_t x509[] PROGMEM = {
  CERTHEX
};

// And so is the key.  These could also be in DRAM 318
static const uint8_t rsakey[] PROGMEM = {
  KEYHEX
};

#endif


