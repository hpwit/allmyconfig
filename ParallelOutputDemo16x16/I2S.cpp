/*
	Author: bitluni 2019
	License: 
	Creative Commons Attribution ShareAlike 2.0
	https://creativecommons.org/licenses/by-sa/2.0/
	
	For further details check out: 
		https://youtube.com/bitlunislab
		https://github.com/bitluni
		http://bitluni.net
*/
#include "I2S.h"
#include "Log.h"
#include <soc/rtc.h>
#define I2S_BASE_CLK (1600000000L)
i2s_dev_t *i2sDevices[] = {&I2S0, &I2S1};

I2S::I2S(const int i2sIndex)
{
	this->i2sIndex = i2sIndex;
	interruptHandle = 0;
	dmaBufferCount = 0;
	dmaBufferActive = 0;
	dmaBuffers = 0;
	stopSignal = false;
}

void IRAM_ATTR I2S::interrupt(void *arg)
{
	volatile i2s_dev_t &i2s = *i2sDevices[((I2S *)arg)->i2sIndex];
	i2s.int_clr.val = i2s.int_raw.val;
    ((I2S *)arg)->callback();
	//((I2S *)arg)->interrupt();
}

void I2S::interrupt()
{
    //Serial.println("interupt");
    //i2sStop();
    //return;
    //Serial.println("interupt");
    //two buufer
    
   dmaBufferActive = (dmaBufferActive + 1);// % dmaBufferCount;
    if(dmaBufferActive==2)
    {
       // Serial.println("on finit");
        i2sStop();
    return;
    }
    /*
	static int c = 0;
	unsigned short *buf = (unsigned short *)dmaBuffers[dmaBufferActive]->buffer;
	/*for (int i = 0; i < 16; i++)
		buf[i] = c++;
	dmaBufferActive = (dmaBufferActive + 1) % dmaBufferCount;
    //Serial.println("ger");
	if (stopSignal)
	{
		i2sStop();
		stopSignal = false;
	}*/
}

void I2S::reset()
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	const unsigned long lc_conf_reset_flags = I2S_IN_RST_M | I2S_OUT_RST_M | I2S_AHBM_RST_M | I2S_AHBM_FIFO_RST_M;
	i2s.lc_conf.val |= lc_conf_reset_flags;
	i2s.lc_conf.val &= ~lc_conf_reset_flags;

	const uint32_t conf_reset_flags = I2S_RX_RESET_M | I2S_RX_FIFO_RESET_M | I2S_TX_RESET_M | I2S_TX_FIFO_RESET_M;
	i2s.conf.val |= conf_reset_flags;
	i2s.conf.val &= ~conf_reset_flags;
	while (i2s.state.rx_fifo_reset_back)
		;
}

void I2S::i2sStop()
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	esp_intr_disable(interruptHandle);
	reset();
	i2s.conf.rx_start = 0;
	i2s.conf.tx_start = 0;
}

void I2S::startTX()
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	DEBUG_PRINTLN("I2S TX");
    //Serial.println("on transmet");
	esp_intr_disable(interruptHandle);
	reset();
	dmaBufferActive = 0;
	DEBUG_PRINT("Sample count ");
 //Serial.println(dmaBuffers[0]->sampleCount());
    i2s.lc_conf.val=I2S_OUT_DATA_BURST_EN | I2S_OUTDSCR_BURST_EN | I2S_OUT_DATA_BURST_EN;
	i2s.out_link.addr = (uint32_t) & (dmaBuffers[0]->descriptor);
	i2s.out_link.start = 1;
    ////vTaskDelay(5);
	i2s.int_clr.val = i2s.int_raw.val;
	i2s.int_ena.val = 0;
	i2s.int_ena.out_eof = 1;
   // //vTaskDelay(5);
	i2s.int_ena.out_dscr_err = 1;
	//enable interrupt
    ////vTaskDelay(5);
	esp_intr_enable(interruptHandle);
   // //vTaskDelay(5);
	//start transmission
	i2s.conf.tx_start = 1;
}

void I2S::startRX()
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	DEBUG_PRINTLN("I2S RX");
	esp_intr_disable(interruptHandle);
	reset();
	dmaBufferActive = 0;
	DEBUG_PRINT("Sample count ");
	DEBUG_PRINTLN(dmaBuffers[0]->sampleCount());
	i2s.rx_eof_num = dmaBuffers[0]->sampleCount();
	i2s.in_link.addr = (uint32_t) & (dmaBuffers[0]->descriptor);
	i2s.in_link.start = 1;
    ////vTaskDelay(5);
	i2s.int_clr.val = i2s.int_raw.val;
	i2s.int_ena.val = 0;
	i2s.int_ena.in_done = 1;
	esp_intr_enable(interruptHandle);
   // //vTaskDelay(5);
	i2s.conf.rx_start = 1;
}

void I2S::resetDMA()
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	i2s.lc_conf.in_rst = 1;
	i2s.lc_conf.in_rst = 0;
	i2s.lc_conf.out_rst = 1;
	i2s.lc_conf.out_rst = 0;
}

void I2S::resetFIFO()
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	i2s.conf.rx_fifo_reset = 1;
	i2s.conf.rx_fifo_reset = 0;
	i2s.conf.tx_fifo_reset = 1;
	i2s.conf.tx_fifo_reset = 0;
}

bool I2S::initParallelInputMode(const int *pinMap, long sampleRate, int baseClock, int wordSelect)
{
    Serial.println("eeee");
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
	//route peripherals
	const int deviceBaseIndex[] = {I2S0I_DATA_IN0_IDX, I2S1I_DATA_IN0_IDX};
	const int deviceClockIndex[] = {I2S0I_BCK_IN_IDX, I2S1I_BCK_IN_IDX};
	const int deviceWordSelectIndex[] = {I2S0I_WS_IN_IDX, I2S1I_WS_IN_IDX};
	const periph_module_t deviceModule[] = {PERIPH_I2S0_MODULE, PERIPH_I2S1_MODULE};
	//works only since indices of the pads are sequential
	for (int i = 0; i < 24; i++)
		if (pinMap[i] > -1)
		{
			PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pinMap[i]], PIN_FUNC_GPIO);
			gpio_set_direction((gpio_num_t)pinMap[i], (gpio_mode_t)GPIO_MODE_DEF_INPUT);
			gpio_matrix_in(pinMap[i], deviceBaseIndex[i2sIndex] + i, false);
		}
	if (baseClock > -1)
		gpio_matrix_in(baseClock, deviceClockIndex[i2sIndex], false);
	if (wordSelect > -1)
		gpio_matrix_in(wordSelect, deviceWordSelectIndex[i2sIndex], false);

	//enable I2S peripheral
	periph_module_enable(deviceModule[i2sIndex]);

	//reset i2s
	i2s.conf.rx_reset = 1;
	i2s.conf.rx_reset = 0;
	i2s.conf.tx_reset = 1;
	i2s.conf.tx_reset = 0;

	resetFIFO();
	resetDMA();

	//parallel mode
	i2s.conf2.val = 0;
	i2s.conf2.lcd_en = 1;
	//from technical datasheet figure 64
	//i2s.conf2.lcd_tx_sdx2_en = 1;
	//i2s.conf2.lcd_tx_wrx2_en = 1;

	i2s.sample_rate_conf.val = 0;
	i2s.sample_rate_conf.rx_bits_mod = 32;

	//maximum rate
	i2s.clkm_conf.val = 0;
	i2s.clkm_conf.clka_en = 0;
	i2s.clkm_conf.clkm_div_num = 6; //3//80000000L / sampleRate;
	i2s.clkm_conf.clkm_div_a = 6;   // 0;
	i2s.clkm_conf.clkm_div_b = 1;   // 0;
	i2s.sample_rate_conf.rx_bck_div_num = 2;

	i2s.fifo_conf.val = 0;
	i2s.fifo_conf.rx_fifo_mod_force_en = 1;
	i2s.fifo_conf.rx_fifo_mod = 3; //byte packing 0A0B_0B0C = 0, 0A0B_0C0D = 1, 0A00_0B00 = 3,
	i2s.fifo_conf.rx_data_num = 32;
	i2s.fifo_conf.dscr_en = 1; //fifo will use dma

	i2s.conf1.val = 0;
	i2s.conf1.tx_stop_en = 1;
	i2s.conf1.tx_pcm_bypass = 1;

	i2s.conf_chan.val = 0;
	i2s.conf_chan.rx_chan_mod = 0;

	//high or low (stereo word order)
	i2s.conf.rx_right_first = 1;

	i2s.timing.val = 0;

	//clear serial mode flags
	i2s.conf.rx_msb_right = 0;
	i2s.conf.rx_msb_shift = 0;
	i2s.conf.rx_mono = 0;
	i2s.conf.rx_short_sync = 0;

	//allocate disabled i2s interrupt
	const int interruptSource[] = {ETS_I2S0_INTR_SOURCE, ETS_I2S1_INTR_SOURCE};
	esp_intr_alloc(interruptSource[i2sIndex], ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, &interrupt, this, &interruptHandle);
	return true;
}

bool I2S::initParallelOutputMode(const int *pinMap, long sampleRate, int baseClock, int wordSelect)
{
	volatile i2s_dev_t &i2s = *i2sDevices[i2sIndex];
    Serial.println("in d");
	//route peripherals
	//in parallel mode only upper 16 bits are interesting in this case
	const int deviceBaseIndex[] = {I2S0O_DATA_OUT0_IDX, I2S1O_DATA_OUT0_IDX};
	const int deviceClockIndex[] = {I2S0O_BCK_OUT_IDX, I2S1O_BCK_OUT_IDX};
	const int deviceWordSelectIndex[] = {I2S0O_WS_OUT_IDX, I2S1O_WS_OUT_IDX};
	const periph_module_t deviceModule[] = {PERIPH_I2S0_MODULE, PERIPH_I2S1_MODULE};
	//works only since indices of the pads are sequential
	for (int i = 0; i < 24; i++)
		if (pinMap[i] > -1)
		{
			PIN_FUNC_SELECT(GPIO_PIN_MUX_REG[pinMap[i]], PIN_FUNC_GPIO);
			gpio_set_direction((gpio_num_t)pinMap[i], (gpio_mode_t)GPIO_MODE_DEF_OUTPUT);
			gpio_matrix_out(pinMap[i], deviceBaseIndex[i2sIndex] + i, false, false);
		}
	if (baseClock > -1)
		gpio_matrix_out(baseClock, deviceClockIndex[i2sIndex], false, false);
	if (wordSelect > -1)
		gpio_matrix_out(wordSelect, deviceWordSelectIndex[i2sIndex], false, false);
Serial.println("in d1");
	//enable I2S peripheral
	periph_module_enable(deviceModule[i2sIndex]);
Serial.println("in d2");
	//reset i2s
	i2s.conf.tx_reset = 1;
	i2s.conf.tx_reset = 0;
	i2s.conf.rx_reset = 1;
	i2s.conf.rx_reset = 0;
Serial.println("in d3");
	resetFIFO();
	resetDMA();
Serial.println("in d4");
	//parallel mode
	i2s.conf2.val = 0;
	i2s.conf2.lcd_en = 1;
    i2s.conf2.lcd_tx_wrx2_en = 0; // HN
    i2s.conf2.lcd_tx_sdx2_en = 0; // HN
	//from technical datasheet figure 64
	//i2s.conf2.lcd_tx_sdx2_en = 1;
	//i2s.conf2.lcd_tx_wrx2_en = 0;

	i2s.sample_rate_conf.val = 0;
	i2s.sample_rate_conf.tx_bits_mod =32; //16
 Serial.println("in d5");
	//clock setup
	//xtal is 40M
	//chip revision 0
	//fxtal * (sdm2 + 4) / (2 * (odir + 2))
	//chip revision 1
	//fxtal * (sdm2 + (sdm1 / 256) + (sdm0 / 65536) + 4) / (2 * (odir + 2))
	//fxtal * (sdm2 + (sdm1 / 256) + (sdm0 / 65536) + 4) needs to be btween 350M and 500M
	//rtc_clk_apll_enable(enable, sdm0, sdm1, sdm2, odir);
	//                           0-255 0-255  0-63  0-31
	//sdm seems to be simply a fixpoint number with 16bits frational part
	//0xA7f00 is the highest value I was able to use. it's just shy of 580MHz. That's a max freq of 145MHz
	//freq = 40000000L * (4 + sdm) / 4
	//sdm = freq / 10000000L - 4;
    int rate=sampleRate;
    int bits=16;
    int clkmInteger, clkmDecimals, bck = 0;
    double denom = (double)1 / 63;
    int channel = 2;
    
    //    double mclk;
    double clkmdiv;
    
    int factor;
    
    if (bits == 8) {
        factor = 120;
    } else {
        factor = (256 % bits) ? 384 : 256;
    }
    
    clkmdiv = (double)I2S_BASE_CLK / (rate* factor);
    if (clkmdiv > 256) {
        log_e("rate is too low");
        return ESP_FAIL;
    }
    //I2S[bus_num].rate = rate;
    
    clkmInteger = clkmdiv;
    clkmDecimals = ((clkmdiv - clkmInteger) / denom);
    
    if (bits == 8) {
        //        mclk = rate* factor;
        bck = 60;
        bits = 16;
    } else {
        //        mclk = (double)clkmInteger + (denom* clkmDecimals);
        bck = factor/(bits* channel);
    }
    Serial.println(sampleRate);
	//long freq = min(sampleRate, 36249999L) * 8; //there are two 1/2 factors in the I2S pipeline for the frequency and another I missed
    //long sdm = 320000;//long(freq * 0.0065536) - 0x40000;
    //Serial.println(sdm);
    /*long freq = sampleRate * 4; //there are two 1/2 factors in the I2S pipeline for the frequency and another I missed
    int sdm, sdmn;
    int odir = -1;
    do
    {
        odir++;
        sdm = long((double(freq) / (20000000. / (odir + 2))) * 0x10000) - 0x40000;
        sdmn = long((double(freq) / (20000000. / (odir + 2 + 1))) * 0x10000) - 0x40000;
    }while(sdm < 0x8c0ecL && odir < 31 && sdmn < 0xA1fff); //0xA7fffL doesn't work on all mcus
        //    sdm = 0x8c0ecL;
    //    odir = 3;
    rtc_clk_apll_enable(true, sdm & 255, (sdm >> 8) & 255, sdm >> 16, odir);*/
     Serial.println("in d6");
	//rtc_clk_apll_enable(true, sdm & 255, (sdm >> 8) & 255, sdm >> 16, 0);
     Serial.println("in d7");
	/*i2s.clkm_conf.val = 0;
	i2s.clkm_conf.clka_en = 0;
    i2s.clkm_conf.clkm_div_num = 40000000L/sampleRate; //; clkmInteger;//80000000L/32000000L;//clkmInteger;//80000000L/100000; //clockN;
	i2s.clkm_conf.clkm_div_a = 3;   //clockA;
    i2s.clkm_conf.clkm_div_b = 2;   //clockB;
    i2s.sample_rate_conf.tx_bck_div_num =3;  //bck;*/
    
    long freq = sampleRate ;//* 2 * (16 / 8);
    int sdm, sdmn;
    int odir = -1;
    do
    {
        odir++;
        sdm = long((double(freq) / (20000000. / (odir + 2))) * 0x10000) - 0x40000;
        sdmn = long((double(freq) / (20000000. / (odir + 2 + 1))) * 0x10000) - 0x40000;
    }while(sdm < 0x8c0ecL && odir < 31 && sdmn < 0xA1fff); //0xA7fffL doesn't work on all mcus
    //DEBUG_PRINTLN(sdm & 255);
    //DEBUG_PRINTLN((sdm >> 8) & 255);
    //DEBUG_PRINTLN(sdm >> 16);
    //DEBUG_PRINTLN(odir);
    //sdm = 0xA1fff;
    //odir = 0;
    if(sdm > 0xA1fff) sdm = 0xA1fff;
   // rtc_clk_apll_enable(true, sdm & 255, (sdm >> 8) & 255, sdm >> 16, odir);
    Serial.println(sdm & 255);
    Serial.println((sdm >> 8) & 255);
    Serial.println(sdm >> 16);
    Serial.println(odir);
   // rtc_clk_apll_enable(true, 1, 0,0 , 0);
    i2s.clkm_conf.val = 0;
    i2s.clkm_conf.clka_en = 0;
    i2s.clkm_conf.clkm_div_num = 33;//33;//1; //clockN;
    i2s.clkm_conf.clkm_div_a = 3;   //clockA;
    i2s.clkm_conf.clkm_div_b =1;   //clockB;
    i2s.sample_rate_conf.tx_bck_div_num = 1;
    

    
    
    
    Serial.printf("div_b %d\n",clkmDecimals);
    Serial.printf("bck_div_num %d\n",bck);
	i2s.fifo_conf.val = 0;
	i2s.fifo_conf.tx_fifo_mod_force_en = 1;
	i2s.fifo_conf.tx_fifo_mod = 3;  //byte packing 0A0B_0B0C = 0, 0A0B_0C0D = 1, 0A00_0B00 = 3,
	i2s.fifo_conf.tx_data_num = 32; //fifo length
	i2s.fifo_conf.dscr_en = 1;		//fifo will use dma

	i2s.conf1.val = 0;
	i2s.conf1.tx_stop_en = 0;
	i2s.conf1.tx_pcm_bypass = 1;

	i2s.conf_chan.val = 0;
    i2s.conf_chan.tx_chan_mod = 2;

	//high or low (stereo word order)
    i2s.conf.tx_right_first = 0;//1;

	i2s.timing.val = 0;

	//clear serial mode flags
	/*i2s.conf.tx_msb_right = 1;
	i2s.conf.tx_msb_shift = 0;
	i2s.conf.tx_mono = 1;
	i2s.conf.tx_short_sync = 0;*/

	//allocate disabled i2s interrupt
	const int interruptSource[] = {ETS_I2S0_INTR_SOURCE, ETS_I2S1_INTR_SOURCE};
    Serial.println("in d5");
	esp_intr_alloc(interruptSource[i2sIndex], ESP_INTR_FLAG_INTRDISABLED | ESP_INTR_FLAG_LEVEL3 | ESP_INTR_FLAG_IRAM, &interrupt, this, &interruptHandle);
    Serial.println("in d6");
	return true;
}

/// simple ringbuffer of blocks of size bytes each
void I2S::allocateDMABuffers(int count, int bytes)
{
	dmaBufferCount = count;
	dmaBuffers = (DMABuffer **)malloc(sizeof(DMABuffer *) * dmaBufferCount);
	if (!dmaBuffers)
		DEBUG_PRINTLN("Failed to allocate DMABuffer array");
	for (int i = 0; i < dmaBufferCount; i++)
	{
		dmaBuffers[i] = DMABuffer::allocate(bytes);
		if (i)
			dmaBuffers[i - 1]->next(dmaBuffers[i]);
	}
	dmaBuffers[dmaBufferCount - 1]->next(dmaBuffers[0]);
}

void I2S::deleteDMABuffers()
{
	if (!dmaBuffers)
		return;
	for (int i = 0; i < dmaBufferCount; i++)
		dmaBuffers[i]->destroy();
	free(dmaBuffers);
	dmaBuffers = 0;
	dmaBufferCount = 0;
}

void I2S::stop()
{
	stopSignal = true;
	while (stopSignal)
		;
}
