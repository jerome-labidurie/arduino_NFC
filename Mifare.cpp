#include "Mifare.h"

static byte packetbuffer[PN532_PACKBUFFSIZE];
static uint8_t uid[] = { 0, 0, 0, 0, 0, 0, 0 };
static uint8_t uidLength;

Mifare::Mifare(){}


/**************************************************************************/
/*!
 @brief  Configures the SAM (Secure Access Module)
 */
/**************************************************************************/

boolean Mifare::SAMConfig() {
    packetbuffer[0] = PN532_COMMAND_SAMCONFIGURATION;
    packetbuffer[1] = 0x01; // normal mode;
    packetbuffer[2] = 0x14; // timeout 50ms * 20 = 1 second
    packetbuffer[3] = 0x01; // use IRQ pin!
    
    if (! board->sendCommandCheckAck(packetbuffer, 4))
        return false;

    // read data packet
    board->readdata(packetbuffer, 8);
    
    return  (packetbuffer[5] == 0x15);
}


/**************************************************************************/
/*!
 Waits for an ISO14443A target to enter the field
  
 @returns a pointer to the uid array or 0 if it fails
 */
/**************************************************************************/
uint8_t* Mifare::readTarget() {

    packetbuffer[0] = PN532_COMMAND_INLISTPASSIVETARGET;
    packetbuffer[1] = 1;  // max 1 cards at once (we can set this to 2 later)
    packetbuffer[2] = MIFARE_ISO14443A; //card baud rate?
    
    if (! board->sendCommandCheckAck(packetbuffer, 3)){
#ifdef PN532DEBUG
        Serial.println("No card(s) read");
#endif
        return 0x0;
    }
    
    uint8_t status = PN532_BUSY;
#ifdef PN532DEBUG
    Serial.println("Waiting for card");
#endif
    while (board->readstatus() != PN532_READY)
    {
        delay(10);
    }
    
#ifdef PN532DEBUG
    Serial.println("Found a card");
#endif
    
    // read data packet
    board->readdata(packetbuffer, 20);
    
    // check some basic stuff
    /* ISO14443A card response should be in the following format:
     
     byte            Description
     -------------   ------------------------------------------
     b0..6           Frame header and preamble
     b7              Tags Found
     b8              Tag Number (only one used in this example)
     b9..10          SENS_RES
     b11             SEL_RES
     b12             NFCID Length
     b13..NFCIDLen   NFCID                                      */
    
#ifdef MIFAREDEBUG
    Serial.print("Found "); Serial.print(packetbuffer[7], DEC); Serial.println(" tags");
#endif
    if (packetbuffer[7] != 1)
        return 0;
    
    uint16_t sens_res = packetbuffer[9];
    sens_res <<= 8;
    sens_res |= packetbuffer[10];
#ifdef MIFAREDEBUG
    Serial.print("Sens Response: 0x");  Serial.println(sens_res, HEX);
    Serial.print("Sel Response: 0x");  Serial.println(packetbuffer[11], HEX);
#endif
    
    uidLength = packetbuffer[12];

    for (uint8_t i=0; i< uidLength; i++) {
       uid[i] = packetbuffer[13+i];
#ifdef MIFAREDEBUG
        Serial.print(" 0x");Serial.print(uid[i], HEX);
#endif
    }
    
    uint32_t cardType = packetbuffer[9] << 16;
    cardType += packetbuffer[10] << 8;
    cardType += packetbuffer[11];
    
        
#ifdef MIFAREDEBUG
    Serial.print("UID:");
    for(uint8_t i=0;i<20;i++){
        Serial.print(packetbuffer[i], HEX); Serial.println(" ");
    }
    Serial.println();
#endif
    
    return uid;
}


/* read payload */

//get type of card and size, then either classic or ultralight read all the blocks



/* write payload */

//get type of card and write the payload using either classic or ultralight

boolean Mifare::writePayload (uint8_t *payload){
   if (!readTarget())
        return false;
    
    switch (cardType) {
        case MIFARE_CLASSIC:
            return classic_writePayload(payload);
            break;
        case MIFARE_ULTRALIGHT:
            return ultralight_writePayload(payload);
            break;
        default:
            return false;
            break;
    }
}


boolean Mifare::classic_writePayload (uint8_t *payload){
    const uint8_t zero[16] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    const uint8_t foot[16] = {0xD3, 0xF7, 0xD3, 0xF7, 0xD3, 0xF7, 0x7F, 0x07, 0x88, 0x40, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    
    uint8_t len = strlen((char*)payload);
    uint8_t block_buffer[16] = {};
    uint8_t start_block = 4;
    
    uint8_t position = 0;
    uint8_t block_count = start_block;
    uint8_t byte_count = 0;
    
    while (position < len){
        if (block_count %4 < 3) {
            //copy payload byte into buffer position
            memcpy (block_buffer+byte_count, payload+position, 1);
            
            byte_count ++;
            position ++;
            
            if (byte_count == 16 ) {
                //end of block
                byte_count = 0;
                
                // write block
                if (!classic_writeMemoryBlock(block_count, block_buffer))
                    return false;
                
                //reset
                memcpy(block_buffer, zero, 16);
                block_count ++;
            }
            
        }else if (block_count %4 == 3) {
            //close sector with footer block
            memcpy(block_buffer, foot, 16);
            if (!classic_writeMemoryBlock(block_count, block_buffer))
                return false;
            memcpy(block_buffer, zero, 16);
            block_count ++;
            byte_count = 0;
        }
    }
    //write any remaining buffer
    if (byte_count > 0){
        if (!classic_writeMemoryBlock(block_count, block_buffer))
            return false;
        block_count ++;
    }
    //fill any empty blocks in the sector
    while (block_count %4 < 3) {
        memcpy(block_buffer, zero, 16);
        if (!classic_writeMemoryBlock(block_count, block_buffer))
            return false;        block_count ++;
    }

    //write final footer block
    memcpy(block_buffer, foot, 16);
    if (!classic_writeMemoryBlock(block_count, block_buffer))
        return false;
    
    return true;
}
boolean Mifare::ultralight_writePayload (uint8_t *payload){
    const uint8_t zero[4] = {0x00, 0x00, 0x00, 0x00};
    
    uint8_t len = strlen((char*)payload);
    uint8_t block_buffer[4] = {};
    uint8_t start_block = 4;
    
    uint8_t position = 0;
    uint8_t block_count = start_block;
    uint8_t byte_count = 0;
    
    
    while (position < len){
        memcpy (block_buffer+byte_count, payload+position, 1);
            
        byte_count ++;
        position ++;
            
        if (byte_count == 4 ) {
            //end of block
            byte_count = 0;
                
            // write block
            if (!ultralight_writeMemoryBlock(block_count, block_buffer))
                return false;
                
            //reset
            memcpy(block_buffer, zero, 16);
            block_count ++;
        }

    }
    //write any remaining buffer
    if (byte_count > 0){
        if (!ultralight_writeMemoryBlock(block_count, block_buffer))
            return false;
    }
   
    return true;
}


/**************************************************************************/
/*!
 Tries to authenticate a block of memory on a MIFARE card using the
 INDATAEXCHANGE command.  See section 7.3.8 of the PN532 User Manual
 for more information on sending MIFARE and other commands.
 
 @param  blockNumber   The block number to authenticate.  (0..63 for
 1KB cards, and 0..255 for 4KB cards).
 @param  keyNumber     Which key type to use during authentication
 (0 = MIFARE_CMD_AUTH_A, 1 = MIFARE_CMD_AUTH_B)
 @param  keyData       Pointer to a byte array containing the 6 byte
 key value
 
 @returns true if everything executed properly, false for an error
 */
/**************************************************************************/
boolean Mifare::classic_authenticateBlock (uint32_t blockNumber){
    
#ifdef MIFAREDEBUG
    Serial.print("Trying to authenticate card ");
#endif
    
    // Prepare the authentication command //
    packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;   /* Data Exchange Header */
    packetbuffer[1] = 1;                              /* Max card numbers */
    packetbuffer[2] = (useKey == KEY_A) ? MIFARE_CMD_AUTH_A : MIFARE_CMD_AUTH_B;
    packetbuffer[3] = blockNumber;                    /* Block Number (1K = 0..63, 4K = 0..255 */
    
    memcpy (packetbuffer+4, (useKey == KEY_A) ? keyA : keyB, 6);
    for (uint8_t i = 0; i < uidLength; i++){
        packetbuffer[10+i] = uid[i];                /* 4 byte card ID */
    }
    
    if (! board->sendCommandCheckAck(packetbuffer, 10+uidLength))
        return false;
    
    // Read the response packet
    board->readdata(packetbuffer, 12);
    
    if((packetbuffer[6] == 0x41) && (packetbuffer[7] == 0x00)) {
        return true;
    }else{
        return false;
    }
}


/**************************************************************************/
/*!
 Tries to read an entire 16-byte data block at the specified block
 address.
 
 @param  blockaddress   The block number to authenticate.  (0..63 for
 1KB cards, and 0..255 for 4KB cards).
 @param  block          Pointer to the byte array that will hold the
 retrieved data (if any)
 
 @returns true if everything executed properly, false for an error
 */
/**************************************************************************/
boolean Mifare::classic_readMemoryBlock(uint8_t blockaddress, uint8_t * block) {
    if (blockaddress >= 64)
        return false;
    
    if (!classic_authenticateBlock (blockaddress)){
        Serial.println("Authentication failed.");
        return false;
    }
    
    packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
    packetbuffer[1] = 1;  // either card 1 or 2 (tested for card 1)
    packetbuffer[2] = MIFARE_CMD_READ;
    packetbuffer[3] = blockaddress; //This address can be 0-63 for MIFARE 1K card
    
    if (! board->sendCommandCheckAck(packetbuffer, 4))
        return false;
    
    // read data packet
    board->readdata(packetbuffer, 18+6);
    // check some basic stuff
#ifdef PN532DEBUG
    Serial.println("READ");
#endif
    for(uint8_t i=8;i<18+6;i++) {
        block[i-8] = packetbuffer[i];
#ifdef PN532DEBUG
        Serial.print(packetbuffer[i], HEX); Serial.print(" ");
#endif
    }
#ifdef PN532DEBUG
    Serial.println("");
#endif
    if((packetbuffer[6] == 0x41) && (packetbuffer[7] == 0x00)){
        return true;
    }else{
        return false;
    }
}


/**************************************************************************/
/*!
 Tries to write an entire 16-byte data block at the specified block
 address.
 
 @param  blockaddress   The block number to authenticate.  (0..63 for
 1KB cards, and 0..255 for 4KB cards).
 @param  block          The byte array that contains the data to write.
 
 @returns true if everything executed properly, false for an error
 */
/**************************************************************************/
//Do not write to Sector Trailer Block unless you know what you are doing.
boolean Mifare::classic_writeMemoryBlock (uint8_t blockaddress, uint8_t * block){
    if (blockaddress >= 64) //64 blocks for a classic
        return false;
    
    if (!classic_authenticateBlock (blockaddress)){
        Serial.println("Authentication failed.");
        return false;
    }
    
    packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
    packetbuffer[1] = 1;  // either card 1 or 2 (tested for card 1)
    packetbuffer[2] = MIFARE_CMD_WRITE;
    packetbuffer[3] = blockaddress;
    
    for(uint8_t byte=0; byte <16; byte++) {
        packetbuffer[4+byte] = block[byte];
    }
    
    if (! board->sendCommandCheckAck(packetbuffer, 20))
        return false;
    // read data packet
    board->readdata(packetbuffer, 2+6);
    
#ifdef PN532DEBUG
    // check some basic stuff
    Serial.println("WRITE");
    for(uint8_t i=0;i<2+6;i++) {
        Serial.print(packetbuffer[i], HEX); Serial.println(" ");
    }
#endif
    
    if((packetbuffer[6] == 0x41) && (packetbuffer[7] == 0x00)) {
        return true; 
    }else{
        return false;
    }
}



/**************************************************************************/
/*!
 Tries to read an entire 4-byte page at the specified address.

 using 'block' for consistency however it refers to a ultralight page here
 
 @param  pageaddress  The page number (0..63 in most cases)
 @param  page         Pointer to the byte array that will hold the
 retrieved data (if any)
 */
/**************************************************************************/
boolean Mifare::ultralight_readMemoryBlock (uint8_t blockaddress, uint8_t *block){
    if (blockaddress >= 64)
        return false;
    
    packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
    packetbuffer[1] = 1;                   /* Card number */
    packetbuffer[2] = MIFARE_CMD_READ;     /* Mifare Read command = 0x30 */
    packetbuffer[3] = blockaddress;         /* Page Number (0..63 in most cases) */
    
    if (! board->sendCommandCheckAck(packetbuffer, 4))
        return false;
    
    /* Read the response packet */
     board->readdata(packetbuffer, 26);
    
#ifdef MIFAREDEBUG
    Serial.println("Received");
#endif
    
    /* If byte 8 isn't 0x00 we probably have an error */
    if (packetbuffer[7] == 0x00) {
        /* Copy the 4 data bytes to the output buffer         */
        /* Block content starts at byte 9 of a valid response */
        /* Note that the command actually reads 16 byte or 4  */
        /* pages at a time ... we simply discard the last 12  */
        /* bytes                                              */
        memcpy (block, packetbuffer+8, 4);
        return true;
    }else{
#ifdef MIFAREDEBUG
        Serial.println("Unexpected response reading block");
#endif
        return false;
    }
}


/**************************************************************************/
/*!
 Tries to read an entire 4-byte page at the specified address.
 
 @param  pageaddress  The page number (0..63 in most cases)
 @param  page         Pointer to the byte array that will hold the
 retrieved data (if any)
 */
/**************************************************************************/

boolean Mifare::ultralight_writeMemoryBlock (uint8_t blockaddress, uint8_t *block){
    if (blockaddress >= 64)
        return false;
    
    packetbuffer[0] = PN532_COMMAND_INDATAEXCHANGE;
    packetbuffer[1] = 1;                   /* Card number */
    packetbuffer[2] = MIFARE_CMD_WRITE;
    packetbuffer[3] = blockaddress;         /* Page Number (0..63 in most cases) */
    
    for(uint8_t byte=0; byte <4; byte++) {
        packetbuffer[4+byte] = block[byte];
    }
    
    if (! board->sendCommandCheckAck(packetbuffer, 8))
        return false;
    
    /* Read the response packet */
    board->readdata(packetbuffer, 8);
    
#ifdef MIFAREDEBUG
    Serial.println("Received");
#endif
    
    if((packetbuffer[6] == 0x41) && (packetbuffer[7] == 0x00)) {
        return true;
    }else{
        return false;
    }}