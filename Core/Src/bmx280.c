/**
  ******************************************************************************
  * File Name          : bmx280.c
  * Description        : This file provides code for the configuration
  *                      and measurment procedures for a BMx280 sensor.
  ******************************************************************************
  * @attention
  *
  ******************************************************************************
  */
  
/* Includes ------------------------------------------------------------------*/
#include "bmx280.h"

/* Locaal variables ---------------------------------------------------------*/
static BMx280_ItemTypeDef bmxItem;
static uint16_t dig_T1  = 0;
static int16_t dig_T2   = 0;
static int16_t dig_T3   = 0;
static uint16_t dig_P1  = 0;
static int16_t dig_P2   = 0;
static int16_t dig_P3   = 0;
static int16_t dig_P4   = 0;
static int16_t dig_P5   = 0;
static int16_t dig_P6   = 0;
static int16_t dig_P7   = 0;
static int16_t dig_P8   = 0;
static int16_t dig_P9   = 0;
static uint8_t dig_H1   = 0;
static int16_t dig_H2   = 0;
static uint8_t dig_H3   = 0;
static int16_t dig_H4   = 0;
static int16_t dig_H5   = 0;
static int8_t dig_H6    = 0;
static BMx280_S32_t t_fine = 0;


/* Global variables ---------------------------------------------------------*/
uint32_t _BMX280REG_    = 0;
int32_t temperature     = 0;
uint32_t pressure       = 0;
uint32_t humidity       = 0;


/* Private function prototypes ----------------------------------------------*/
static ErrorStatus BMx280_Read(BMx280_ItemTypeDef item, uint8_t cmd, uint8_t *buf, uint8_t len);
static ErrorStatus BMx280_Write(BMx280_ItemTypeDef item, uint8_t *buf, uint8_t len);
static BMx280_S32_t bmx280_compensate_T_int32(BMx280_S32_t adc_T);
static BMx280_U32_t bmx280_compensate_P_int32(BMx280_S32_t adc_P);
static BMx280_U32_t bmx280_compensate_H_int32(BMx280_S32_t adc_H);













////////////////////////////////////////////////////////////////////////////////
/**
  * @brief  Initialize a sensor
  * @param  sensorType: a model of a sensor
  * @param  transportType: a data transport of a sensor
  * @return Error status
  */
ErrorStatus BMx280_Init(BMx280_SensorTypeDef sensorType, BMx280_TransportTypeDef transportType) {
  uint8_t buf[32];

  bmxItem.sensorType = sensorType;
  bmxItem.transportType = transportType;

  // ErrorStatus (*Callback_Read)();
  // ErrorStatus (*Callback_Write)();

  // if (transportType == BMx280_I2C) {
  //   Callback_Read = &I2C_Read;
  //   Callback_Write = &I2C_Write;
  // } else {
  //   // callback = &SPI_read;
  // }

  // if (Callback_Read(I2C1, BMX280_I2C_ADDR, BMX280_DEV_ID, buf, 1)) {
  //   return (ERROR);
  // }

  /* Read Device ID and if it isn't equal to the current, exit. */
  buf[0] = 0;
  if (BMx280_Read(bmxItem, BMX280_DEV_ID, buf, 1)) {
    return (ERROR);
  }
  if (sensorType != buf[0]) {
    return (ERROR);
  }

  /* Read calibration data. This is common for both sensor's types */
  if (BMx280_Read(bmxItem, BMX280_CALIB1, buf, 26)) {
    return (ERROR);
  }
  dig_T1 = (uint16_t)((buf[1] << 8) | buf[0]);
  dig_T2 = (int16_t)((buf[3] << 8) | buf[2]);
  dig_T3 = (int16_t)((buf[5] << 8) | buf[4]);
  dig_P1 = (uint16_t)((buf[7] << 8) | buf[6]);
  dig_P2 = (int16_t)((buf[9] << 8) | buf[8]);
  dig_P3 = (int16_t)((buf[11] << 8) | buf[10]);
  dig_P4 = (int16_t)((buf[13] << 8) | buf[12]);
  dig_P5 = (int16_t)((buf[15] << 8) | buf[14]);
  dig_P6 = (int16_t)((buf[17] << 8) | buf[16]);
  dig_P7 = (int16_t)((buf[19] << 8) | buf[18]);
  dig_P8 = (int16_t)((buf[21] << 8) | buf[20]);
  dig_P9 = (int16_t)((buf[23] << 8) | buf[22]);

  /* If the sensor is BME280, read additianal block of calibration data */
  if (sensorType == BME280) {
    dig_H1 = (uint8_t)buf[25];

    if (BMx280_Read(bmxItem, BMX280_CALIB2, buf, 16)) {
      return (ERROR);
    }
    dig_H2 = (int16_t)((buf[1] << 8) | buf[0]);
    dig_H3 = (uint8_t)buf[2];
    dig_H4 = (int16_t)((buf[3] << 4) | (buf[4] & 0x0f));
    dig_H5 = (int16_t)((buf[5] << 4) | ((buf[4] >> 4) & 0x0f));
    dig_H6 = (int8_t)buf[6];
  }

  /* Set filter coefficient to 8 and 1s inactive duration in normal mode */
  buf[0] = BMX280_SETTINGS;
  buf[1] = 0xac;
  if (BMx280_Write(bmxItem, buf, 2)) {
    return (ERROR);
  }

  /* Set the humidity oversampling to 16 (highest precision) */
  buf[0] = BMX280_CTRL_HUM;
  buf[1] = 0x05;
  if (BMx280_Write(bmxItem, buf, 2)) {
    return (ERROR);
  }

  /* Set the temperature and pressure oversampling to 16 (highest precision) */
  buf[0] = BMX280_CTRL_MEAS;
  buf[1] = 0xb4;
  if (BMx280_Write(bmxItem, buf, 2)) {
    return (ERROR);
  }
  return (SUCCESS);
}




/**
  * @brief  Runs a measurement proccess
  * @param  None
  * @return Error status
  */
ErrorStatus BMx280_Measurment(void) {
  uint8_t buf[8];

  /* Run conversion in forse mode, keep oversampling */
  buf[0] = BMX280_CTRL_MEAS;
  buf[1] = 0xb5;
  if (BMx280_Write(bmxItem, buf, 2)) {
    return (ERROR);
  }

  /* Read the status register and check measuring busy flag, wait for conversion */
  if (BMx280_Read(bmxItem, BMX280_STATUS, buf, 1)) {
    return (ERROR);
  }
  if (buf[0] == 0x08) {
    Delay(20);
  }

  /* Read raw data */
  if (BMx280_Read(bmxItem, BMX280_DATA, buf, 8)) {
    return (ERROR);
  }

  BMx280_S32_t adc_P = (((buf[0] << 8) | buf[1]) << 4) | (buf[2] >> 4);
  BMx280_S32_t adc_T = (((buf[3] << 8) | buf[4]) << 4) | (buf[5] >> 4);
  BMx280_S32_t adc_H = (buf[6] << 8) | buf[7];

  temperature = bmx280_compensate_T_int32(adc_T);
  // printf("%ld\n", temperature);

  pressure = bmx280_compensate_P_int32(adc_P);
  // printf("%ld\n", pressure);

  humidity = bmx280_compensate_H_int32(adc_H);
  // printf("%ld\n", humidity);

  return (SUCCESS);
}




/**
  * @brief  Reads BMx280 data
  * @param  item: BMx280 item to exploit for reading
  * @param  reg: a register to read from BMx280
  * @param  buf: a pointer of a buffer which is for store data
  * @param  len: length of the buffer
  * @return Error status
  */
static ErrorStatus BMx280_Read(BMx280_ItemTypeDef item, uint8_t reg, uint8_t *buf, uint8_t len) {
  if (item.transportType == BMx280_I2C) {
    if (I2C_Read(I2C1, BMX280_I2C_ADDR, reg, buf, len)) {
      return (ERROR);
    }
  } else {
    // ToDo SPI read handler
  }
  return (SUCCESS);
}




/**
  * @brief  Writess BMx280 data
  * @param  item: BMx280 item to exploit for writing
  * @param  buf: a pointer of a buffer when sending data is stored 
  *         (the first item should contain a BMx280 register address)
  * @param  len: length of the buffer
  * @return Error status
  */
static ErrorStatus BMx280_Write(BMx280_ItemTypeDef item, uint8_t *buf, uint8_t len) {
  if (item.transportType == BMx280_I2C) {
    if (I2C_Write(I2C1, BMX280_I2C_ADDR, buf, len)) {
      return (ERROR);
    }
  } else {
    // ToDo SPI write handler
  }
  return (SUCCESS);
}




/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/*
  Here is a bunch of functions provided by Bosch sensor engineering 
  and proposed by BMx280 datasheet. They were set here with minor
  changed. The logic was kept.
  
  t_fine carries fine temperature as global value
*/

/**
  * @brief  Returns temperature in DegC, resolution is 0.01 DegC.
  *         Output value of “5123” equals 51.23 DegC.
  * @param  adc_T: Raw temperature data
  * @return Measured temprerature
  */
static BMx280_S32_t bmx280_compensate_T_int32(BMx280_S32_t adc_T) {
  BMx280_S32_t var1, var2, T;
  var1 = ((((adc_T >> 3) - ((BMx280_S32_t)dig_T1 << 1))) * (BMx280_S32_t)dig_T2) >> 11;
  var2 = (((((adc_T >> 4) - (BMx280_S32_t)dig_T1) * ((adc_T >> 4) - (BMx280_S32_t)dig_T1)) >> 12) * (BMx280_S32_t)dig_T3) >> 14;
  t_fine = var1 + var2;

  T = (t_fine * 5 + 128) >> 8;
  return (T);
}



/**
  * @brief  Returns pressure in Pa as unsigned 32 bit integer.
  *         Output value of “96386” equals 96386 Pa = 963.86 hPa
  * @param  adc_P: Raw pressure data
  * @return Measured pressure
  */
static BMx280_U32_t bmx280_compensate_P_int32(BMx280_S32_t adc_P) {
  BMx280_S32_t var1, var2;
  BMx280_U32_t P;
  var1 = (((BMx280_S32_t)t_fine) >> 1) - (BMx280_S32_t)64000;
  var2 = (((var1 >> 2) * (var1 >> 2)) >> 11 ) * ((BMx280_S32_t)dig_P6);
  var2 = var2 + ((var1 * ((BMx280_S32_t)dig_P5)) << 1);
  var2 = (var2 >> 2) + (((BMx280_S32_t)dig_P4) << 16);
  var1 = (((dig_P3 * (((var1 >> 2) * (var1 >> 2)) >> 13 )) >> 3) + ((((BMx280_S32_t)dig_P2) * var1) >> 1)) >> 18;
  var1 = ((((32768 + var1)) * ((BMx280_S32_t)dig_P1)) >> 15);

  if (var1 == 0) return (0); // avoid excePtion caused by division by zero

  P = (((BMx280_U32_t)(((BMx280_S32_t)1048576) - adc_P) - (var2 >> 12))) * 3125;
  if (P < 0x80000000) {
    P = (P << 1) / ((BMx280_U32_t)var1);
  } else {
    P = (P / (BMx280_U32_t)var1) * 2;
  }

  var1 = (((BMx280_S32_t)dig_P9) * ((BMx280_S32_t)(((P >> 3) * (P >> 3)) >> 13))) >> 12;
  var2 = (((BMx280_S32_t)(P >> 2)) * ((BMx280_S32_t)dig_P8)) >> 13;

  P = (BMx280_U32_t)((BMx280_S32_t)P + ((var1 + var2 + dig_P7) >> 4));
  return (P);
}




/**
  * @brief  Returns humidity in %RH as unsigned 32 bit integer in Q22.10 format (22 integer and 10 fractional bits).
  *         Output value of “47445” represents 47445/1024 = 46.333 %RH
  * @param  adc_H: Raw humidity data
  * @return Measured humidity
  */
static BMx280_U32_t bmx280_compensate_H_int32(BMx280_S32_t adc_H) {
  // varant 2
  BMx280_U32_t H;
  H = (BMx280_S32_t)t_fine - (BMx280_S32_t)76800;
  H = (((((adc_H << 14) - ((BMx280_S32_t)dig_H4 << 20) - ((BMx280_S32_t)dig_H5 * H)) + 16384) >> 15) * ((((((((BMx280_S32_t)H * dig_H6) >> 10) * ((((BMx280_S32_t)H * dig_H3) >> 11) + 32768)) >> 10) + 2097152) * (BMx280_S32_t)dig_H2 + 8192) >> 14));
  H = (H - (((((H >> 15) * (H >> 15)) >> 7) * (BMx280_S32_t)dig_H1) >> 4));

  if (H > 0x19000000) H = 0x19000000;
  if (H < 0) H = 0;

  H >>= 12;
  return (H);
}

/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
/* ------------------------------------------------------------------------------- */
