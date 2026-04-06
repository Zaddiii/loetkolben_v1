#include "storage.h"

#include "main.h"

#include <stddef.h>
#include <string.h>

enum
{
  STORAGE_MAGIC = 0x534F4C44UL,
  STORAGE_VERSION = 1UL,
  STORAGE_FLAG_CALIBRATION_VALID = (1UL << 0),
  STORAGE_FLASH_ADDRESS = 0x080E0000UL,
  STORAGE_FLASH_BANK = FLASH_BANK_1,
  STORAGE_FLASH_SECTOR = FLASH_SECTOR_7,
  STORAGE_FLASH_SECTOR_COUNT = 1U,
  STORAGE_FLASHWORD_SIZE = 32U,
  STORAGE_ERROR_NONE = 0U,
  STORAGE_ERROR_INVALID_MAGIC = 1U,
  STORAGE_ERROR_INVALID_VERSION = 2U,
  STORAGE_ERROR_INVALID_CRC = 3U,
  STORAGE_ERROR_FLASH_UNLOCK = 4U,
  STORAGE_ERROR_FLASH_ERASE = 5U,
  STORAGE_ERROR_FLASH_PROGRAM = 6U,
  STORAGE_ERROR_INVALID_ARGUMENT = 7U
};

typedef struct __attribute__((aligned(32)))
{
  StorageImage image;
  uint8_t padding[32U];
} StorageFlashImage;

static StorageContext storage_context;

static uint32_t Storage_ComputeCrc32(const uint8_t *data, size_t length)
{
  uint32_t crc = 0xFFFFFFFFUL;
  size_t index;

  for (index = 0; index < length; ++index)
  {
    uint32_t current = data[index];
    uint8_t bit_index;

    crc ^= current;

    for (bit_index = 0U; bit_index < 8U; ++bit_index)
    {
      uint32_t mask = (uint32_t)-(int32_t)(crc & 1UL);
      crc = (crc >> 1U) ^ (0xEDB88320UL & mask);
    }
  }

  return ~crc;
}

static void Storage_SetDefaultImage(void)
{
  memset(&storage_context.image, 0, sizeof(storage_context.image));
  storage_context.image.magic = STORAGE_MAGIC;
  storage_context.image.version = STORAGE_VERSION;
  storage_context.image.target_temp_cdeg = 3200U;
}

static bool Storage_IsImageValid(const StorageImage *image)
{
  uint32_t crc32;

  if (image->magic != STORAGE_MAGIC)
  {
    storage_context.last_error = STORAGE_ERROR_INVALID_MAGIC;
    return false;
  }

  if (image->version != STORAGE_VERSION)
  {
    storage_context.last_error = STORAGE_ERROR_INVALID_VERSION;
    return false;
  }

  crc32 = Storage_ComputeCrc32((const uint8_t *)image, offsetof(StorageImage, crc32));
  storage_context.last_crc32 = crc32;

  if (crc32 != image->crc32)
  {
    storage_context.last_error = STORAGE_ERROR_INVALID_CRC;
    return false;
  }

  storage_context.last_error = STORAGE_ERROR_NONE;
  return true;
}

static void Storage_UpdateCrc(StorageImage *image)
{
  image->crc32 = Storage_ComputeCrc32((const uint8_t *)image, offsetof(StorageImage, crc32));
  storage_context.last_crc32 = image->crc32;
}

static void Storage_ExportCalibrationTable(const CalibrationTable *table, StorageImage *image)
{
  uint8_t index;

  image->flags &= ~STORAGE_FLAG_CALIBRATION_VALID;
  image->calibration_valid = 0U;
  image->point_count = 0U;

  for (index = 0U; index < CALIBRATION_MAX_POINTS; ++index)
  {
    image->points[index].tip_temp_cdeg = 0U;
    image->points[index].internal_raw = 0U;
    image->points[index].valid = 0U;
    memset(image->points[index].reserved, 0, sizeof(image->points[index].reserved));
  }

  if (table == NULL)
  {
    return;
  }

  image->point_count = table->point_count;
  image->calibration_valid = table->valid;

  if (table->valid != 0U)
  {
    image->flags |= STORAGE_FLAG_CALIBRATION_VALID;
  }

  for (index = 0U; (index < table->point_count) && (index < CALIBRATION_MAX_POINTS); ++index)
  {
    image->points[index].tip_temp_cdeg = table->points[index].tip_temp_cdeg;
    image->points[index].internal_raw = table->points[index].internal_raw;
    image->points[index].valid = table->points[index].valid;
  }
}

static bool Storage_WriteImage(void)
{
  FLASH_EraseInitTypeDef erase_init = {0};
  StorageFlashImage flash_image;
  uint32_t sector_error = 0U;
  uint32_t address = STORAGE_FLASH_ADDRESS;
  size_t offset;

  memset(&flash_image, 0xFF, sizeof(flash_image));
  flash_image.image = storage_context.image;
  Storage_UpdateCrc(&flash_image.image);
  storage_context.image.crc32 = flash_image.image.crc32;

  if (HAL_FLASH_Unlock() != HAL_OK)
  {
    storage_context.last_error = STORAGE_ERROR_FLASH_UNLOCK;
    return false;
  }

  erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
  erase_init.Banks = STORAGE_FLASH_BANK;
  erase_init.Sector = STORAGE_FLASH_SECTOR;
  erase_init.NbSectors = STORAGE_FLASH_SECTOR_COUNT;

  if (HAL_FLASHEx_Erase(&erase_init, &sector_error) != HAL_OK)
  {
    (void)HAL_FLASH_Lock();
    storage_context.last_error = STORAGE_ERROR_FLASH_ERASE;
    return false;
  }

  for (offset = 0U; offset < sizeof(flash_image); offset += STORAGE_FLASHWORD_SIZE)
  {
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD,
                          address + (uint32_t)offset,
                          (uint32_t)((uintptr_t)&flash_image + offset)) != HAL_OK)
    {
      (void)HAL_FLASH_Lock();
      storage_context.last_error = STORAGE_ERROR_FLASH_PROGRAM;
      return false;
    }
  }

  (void)HAL_FLASH_Lock();
  storage_context.last_error = STORAGE_ERROR_NONE;
  storage_context.flash_data_valid = 1U;
  storage_context.dirty = 0U;
  return true;
}

bool Storage_Init(void)
{
  const StorageImage *flash_image = (const StorageImage *)STORAGE_FLASH_ADDRESS;

  memset(&storage_context, 0, sizeof(storage_context));
  Storage_SetDefaultImage();
  storage_context.initialized = 1U;

  if (Storage_IsImageValid(flash_image))
  {
    storage_context.image = *flash_image;
    storage_context.flash_data_valid = 1U;
    return true;
  }

  Storage_SetDefaultImage();
  return false;
}

bool Storage_SaveTargetTemp(uint16_t target_temp_cdeg)
{
  if (storage_context.initialized == 0U)
  {
    (void)Storage_Init();
  }

  storage_context.image.target_temp_cdeg = target_temp_cdeg;
  storage_context.dirty = 1U;
  return Storage_WriteImage();
}

bool Storage_SaveCalibrationTable(const CalibrationTable *table)
{
  if (storage_context.initialized == 0U)
  {
    (void)Storage_Init();
  }

  if (table == NULL)
  {
    storage_context.last_error = STORAGE_ERROR_INVALID_ARGUMENT;
    return false;
  }

  Storage_ExportCalibrationTable(table, &storage_context.image);
  storage_context.dirty = 1U;
  return Storage_WriteImage();
}

const StorageContext *Storage_GetContext(void)
{
  return &storage_context;
}