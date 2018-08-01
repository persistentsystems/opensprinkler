//To compile --gcc test.c -o test -L /usr/local/ssl/lib -lssl -lcrypto -Wall

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>

#include<stdio.h>


void writeArray(FILE *fp, unsigned char *buff, int len){
		//opening breaket for array
		fprintf(fp, "{\n");
    for(int i =0; i<len; i++){
    	fprintf(fp, "0x%02x", buff[i]);

    	if(i == len-1){
    		//new line at end
    		fprintf(fp, "\n");
    	}
    	else{
    		//comma and tab between each byte
    		fprintf(fp, ",\t");
    	}

    	if((i+1)%12 == 0){
    		//new line after every 12 bytes
    		fprintf(fp, "\n");
    	}

    }
    //closing bracket and semicomma
    fprintf(fp, "};\n");
}

int main (int argc, char *argv[]) 
{
		//parse arguments
		if(argc != 4 ){
			printf("Invalid arguments.. \n");
			printf("./convertCertificate key_file.key cert_file.cert header_file.h\n");
			exit(1);
		}
		char *keypath = argv[1];
		char *certpath = argv[2];
		char *outpath = argv[3];
		
		//open output header file for writing
		FILE *fp;
		fp = fopen(outpath, "w");
		if(fp == NULL){
			printf("Failed to create file\n");
			exit(1);
		}
		printf("File %s opened for writing...\n", outpath);
		
		//convert certificate file in buffer array
		X509 *x509;    
    BIO *certBio = BIO_new(BIO_s_file());
    int certlen;
    unsigned char *certbuf = NULL;
    
    printf("Loading certificate %s ....\n", certpath);
    certbuf = NULL;
    BIO_read_filename(certBio, certpath);
    x509 = PEM_read_bio_X509_AUX(certBio, NULL, 0, NULL);      /*loading the certificate to x509 structure*/
    certlen = i2d_X509(x509, &certbuf);     /*loading the certificate to unsigned char buffer*/
    
    //write certificate byte array in C array 
    printf("Certificate size = %d\n", certlen);
    printf("Writing certificate array....\n");
    fprintf(fp, "//The certificate is stored in PMEM \nstatic const uint8_t x509[] PROGMEM = \n");
    writeArray(fp, certbuf, certlen);	

		//convert key file in buffer array
		EVP_PKEY *pkey = NULL;
    BIO *pkeybio = BIO_new(BIO_s_file());
    unsigned char *keybuf;
   	int keylen;   	
   	
   	printf("Loading key %s....\n", keypath);
   	int ret = BIO_read_filename(pkeybio, keypath);
   	if (!(pkey = PEM_read_bio_PrivateKey(pkeybio, NULL, 0, NULL))) {
	    printf("Error loading private key into memory\n");
  	  exit(-1);
  	}
  	keylen = i2d_PrivateKey(pkey, NULL);
		printf("Key size = %d\n", keylen);
		
		keybuf =(unsigned char *) malloc(keylen + 1);
		if(keybuf ==NULL){
			printf("Failed to allocate memory for key\n");
			exit(1);
		}
  	keylen = i2d_PrivateKey(pkey, &keybuf);
  	
		//write key array in file
    fprintf(fp, "// And so is the key.  These could also be in DRAM size =%d\nstatic const uint8_t rsakey[] PROGMEM =\n", keylen);
    writeArray(fp, keybuf, keylen);	
   	printf("Conversion completed...\n");
   	
		fclose(fp);
    
    return 0;
} 
