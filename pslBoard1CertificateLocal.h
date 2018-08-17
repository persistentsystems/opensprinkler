//The certificate is stored in PMEM 
static const uint8_t x509[] PROGMEM = 
{
0x30, 0x82, 0x01, 0x70, 0x30, 0x82, 0x01, 0x1a, 0x02, 0x09, 0x00, 0xc7, 0x97, 0x49, 0x52, 0x1f, 0x8c, 0xb7, 0xa5, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x30, 0x3f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x02, 0x43, 0x41, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x03, 0x33, 0x50, 0x45, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0c, 0x31, 0x30, 0x2e, 0x34, 0x34, 0x2e, 0x31, 0x36, 0x32, 0x2e, 0x36, 0x39, 0x30, 0x1e, 0x17, 0x0d, 0x31, 0x38, 0x30, 0x38, 0x30, 0x37, 0x31, 0x36, 0x34, 0x33, 0x33, 0x30, 0x5a, 0x17, 0x0d, 0x31, 0x39, 0x30, 0x38, 0x30, 0x37, 0x31, 0x36, 0x34, 0x33, 0x33, 0x30, 0x5a, 0x30, 0x3f, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x06, 0x13, 0x02, 0x55, 0x53, 0x31, 0x0b, 0x30, 0x09, 0x06, 0x03, 0x55, 0x04, 0x08, 0x0c, 0x02, 0x43, 0x41, 0x31, 0x0c, 0x30, 0x0a, 0x06, 0x03, 0x55, 0x04, 0x0a, 0x0c, 0x03, 0x33, 0x50, 0x45, 0x31, 0x15, 0x30, 0x13, 0x06, 0x03, 0x55, 0x04, 0x03, 0x0c, 0x0c, 0x31, 0x30, 0x2e, 0x34, 0x34, 0x2e, 0x31, 0x36, 0x32, 0x2e, 0x36, 0x39, 0x30, 0x5c, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x01, 0x05, 0x00, 0x03, 0x4b, 0x00, 0x30, 0x48, 0x02, 0x41, 0x00, 0xc8, 0xb0, 0xd3, 0x78, 0xf2, 0x77, 0xb5, 0x0b, 0x5f, 0xfc, 0xc8, 0x3c, 0x43, 0x4c, 0xab, 0x63, 0x57, 0x27, 0x87, 0xa2, 0x61, 0x15, 0xe4, 0xd1, 0x95, 0x85, 0x55, 0xb3, 0xfa, 0x23, 0xd3, 0x6b, 0x3b, 0xcb, 0x4f, 0x70, 0x65, 0x85, 0x23, 0xe0, 0x4e, 0xbd, 0xf8, 0xfc, 0x79, 0xaf, 0x6e, 0x05, 0x91, 0xe2, 0xc8, 0xc9, 0xbe, 0x76, 0x86, 0x1a, 0x7e, 0x3a, 0x01, 0x8a, 0xad, 0x72, 0xf7, 0x49, 0x02, 0x03, 0x01, 0x00, 0x01, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b, 0x05, 0x00, 0x03, 0x41, 0x00, 0x67, 0x79, 0xa6, 0x4d, 0xc3, 0x30, 0x30, 0x5d, 0x59, 0x4d, 0x8c, 0xfc, 0x6e, 0x89, 0x31, 0x5c, 0x96, 0xe5, 0x87, 0xbd, 0x87, 0x3c, 0x7e, 0x5e, 0x0a, 0xac, 0x0b, 0x94, 0xfe, 0x3e, 0xb9, 0xe1, 0xb5, 0x2e, 0xe2, 0xcb, 0xf0, 0x3d, 0xff, 0x9b, 0x92, 0x9f, 0xa6, 0xec, 0x65, 0xc5, 0xee, 0x03, 0xd4, 0x55, 0xd1, 0x8d, 0xdf, 0xe2, 0xdb, 0xf1, 0xf1, 0xb9, 0x8d, 0xfa, 0x7e, 0xaf, 0x96, 0x47, 
};
// And so is the key.  These could also be in DRAM 
static const uint8_t rsakey[] PROGMEM =
{
0x30, 0x82, 0x01, 0x3a, 0x02, 0x01, 0x00, 0x02, 0x41, 0x00, 0xc8, 0xb0, 0xd3, 0x78, 0xf2, 0x77, 0xb5, 0x0b, 0x5f, 0xfc, 0xc8, 0x3c, 0x43, 0x4c, 0xab, 0x63, 0x57, 0x27, 0x87, 0xa2, 0x61, 0x15, 0xe4, 0xd1, 0x95, 0x85, 0x55, 0xb3, 0xfa, 0x23, 0xd3, 0x6b, 0x3b, 0xcb, 0x4f, 0x70, 0x65, 0x85, 0x23, 0xe0, 0x4e, 0xbd, 0xf8, 0xfc, 0x79, 0xaf, 0x6e, 0x05, 0x91, 0xe2, 0xc8, 0xc9, 0xbe, 0x76, 0x86, 0x1a, 0x7e, 0x3a, 0x01, 0x8a, 0xad, 0x72, 0xf7, 0x49, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x41, 0x00, 0x82, 0x76, 0x08, 0x92, 0xc8, 0x34, 0x2f, 0x39, 0xdc, 0xc5, 0x2b, 0xb9, 0x99, 0x1a, 0x3f, 0x13, 0xcd, 0xf5, 0x41, 0x83, 0xba, 0x4f, 0x0c, 0x37, 0x7e, 0x56, 0x75, 0xf7, 0x10, 0x75, 0xb7, 0x9a, 0x08, 0x41, 0x70, 0xe9, 0x2b, 0xb2, 0xc4, 0xd6, 0xed, 0x2e, 0x5d, 0x5d, 0x3e, 0x77, 0xab, 0xd2, 0xd9, 0x87, 0xc2, 0xc8, 0x8d, 0xc6, 0x4e, 0x4c, 0x6a, 0x00, 0xbc, 0xe3, 0x09, 0x96, 0x5b, 0x5d, 0x02, 0x21, 0x00, 0xe6, 0x6e, 0x0b, 0xbe, 0xa2, 0x77, 0x0e, 0x3a, 0xa1, 0x49, 0x3c, 0x47, 0x67, 0xd9, 0xbc, 0x6b, 0xd2, 0xdf, 0x65, 0xb0, 0xbb, 0x6d, 0xde, 0x66, 0x4a, 0xb7, 0x30, 0x07, 0x2b, 0xc1, 0xe7, 0x87, 0x02, 0x21, 0x00, 0xde, 0xf5, 0xf6, 0x9a, 0x07, 0x63, 0xac, 0x4c, 0xc0, 0x30, 0xb1, 0xbf, 0xd2, 0x1a, 0xa9, 0xb5, 0x29, 0xb5, 0xf5, 0xad, 0xb4, 0xd5, 0x7e, 0x8b, 0x4e, 0xf2, 0x71, 0x0f, 0x22, 0x55, 0x3e, 0xaf, 0x02, 0x20, 0x4d, 0x85, 0x78, 0x2e, 0x0a, 0x3a, 0x43, 0x6f, 0x36, 0x13, 0x8d, 0x53, 0xf8, 0x7c, 0x28, 0x07, 0x9e, 0x49, 0xc9, 0xcc, 0x4b, 0x42, 0x0b, 0x30, 0x1e, 0xb2, 0xc2, 0x55, 0xa7, 0x42, 0xf4, 0xc1, 0x02, 0x20, 0x1b, 0xef, 0x2f, 0x25, 0x5f, 0x35, 0xa2, 0xb9, 0xbe, 0xfe, 0x9a, 0xd6, 0x90, 0xa9, 0x5c, 0x87, 0xe1, 0x20, 0xf6, 0x15, 0xfc, 0x69, 0x1f, 0x40, 0xae, 0xf4, 0x7b, 0x2b, 0xcd, 0x3e, 0x66, 0xeb, 0x02, 0x20, 0x71, 0xbf, 0xba, 0xc0, 0x04, 0x2d, 0x2f, 0x30, 0x79, 0x4b, 0x21, 0xec, 0xe0, 0x20, 0xbc, 0xce, 0xfa, 0x2a, 0x36, 0x09, 0xcb, 0x9c, 0x64, 0xc4, 0xed, 0xa5, 0xc4, 0x0f, 0x23, 0xc2, 0x98, 0x08, 
};