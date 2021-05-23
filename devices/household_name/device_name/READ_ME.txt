// Run these commands:
openssl ecparam -genkey -name prime256v1 -noout -out ec_private.pem
openssl ec -in ec_private.pem -pubout -out ec_public.pem

// Run the command below and copy priv: part to thing_info (put it in one line).
// The key length should be exactly the same as the key length bellow (32 pairs
// of hex digits). If it's bigger and it starts with "00:" delete the "00:". If
// it's smaller add "00:" to the start. If it's too big or too small something
// is probably wrong with your key.
openssl ec -in ec_private.pem -noout -text

// Create a device and assign a public key to it.
gcloud iot devices create DEVICE_NAME --region=us-central1 --registry=atest-registry --public-key path=ec_public.pem,type=es256

; cd ../../../
; pio run --target uploadfs -e sapir
; pio run --target upload -e sapir


Don't forget to add the device to the Lambda/Function in GCP