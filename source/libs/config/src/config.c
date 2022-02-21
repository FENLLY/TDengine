/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#define _DEFAULT_SOURCE
#include "cfgInt.h"

SConfig *cfgInit() {
  SConfig *pConfig = calloc(1, sizeof(SConfig));
  if (pConfig == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return NULL;
  }

  pConfig->hash = taosHashInit(64, MurmurHash3_32, false, HASH_NO_LOCK);
  if (pConfig->hash == NULL) {
    free(pConfig);
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return NULL;
  }

  return pConfig;
}

int32_t cfgLoad(SConfig *pConfig, ECfgSrcType cfgType, const char *sourceStr) {
  switch (cfgType) {
    case CFG_TYPE_CFG_FILE:
      return cfgLoadFromCfgFile(pConfig, sourceStr);
    case CFG_TYPE_ENV_FILE:
      return cfgLoadFromEnvFile(pConfig, sourceStr);
    case CFG_TYPE_ENV_VAR:
      return cfgLoadFromEnvVar(pConfig);
    case CFG_TYPE_APOLLO_URL:
      return cfgLoadFromApollUrl(pConfig, sourceStr);
    default:
      return -1;
  }
}

void cfgCleanup(SConfig *pConfig) {
  if (pConfig != NULL) {
    if (pConfig->hash != NULL) {
      taosHashCleanup(pConfig->hash);
      pConfig->hash == NULL;
    }
    free(pConfig);
  }
}

int32_t cfgGetSize(SConfig *pConfig) { return taosHashGetSize(pConfig->hash); }

SConfigItem *cfgIterate(SConfig *pConfig, SConfigItem *pIter) { return taosHashIterate(pConfig->hash, pIter); }

void cfgCancelIterate(SConfig *pConfig, SConfigItem *pIter) { return taosHashCancelIterate(pConfig->hash, pIter); }

SConfigItem *cfgGetItem(SConfig *pConfig, const char *name) { taosHashGet(pConfig->hash, name, strlen(name) + 1); }

static int32_t cfgAddItem(SConfig *pConfig, SConfigItem *pItem, const char *name, ECfgUnitType utype) {
  pItem->stype = CFG_TYPE_DEFAULT;
  pItem->utype = utype;
  pItem->name = strdup(name);
  if (pItem->name == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  if (taosHashPut(pConfig->hash, name, strlen(name) + 1, pItem, sizeof(SConfigItem)) != 0) {
    if (pItem->dtype == CFG_DTYPE_STRING) {
      free(pItem->strVal);
    } else if (pItem->dtype == CFG_DTYPE_FQDN) {
      free(pItem->fqdnVal);
    } else if (pItem->dtype == CFG_DTYPE_IPSTR) {
      free(pItem->ipstrVal);
    } else if (pItem->dtype == CFG_DTYPE_DIR) {
      free(pItem->dirVal);
    } else if (pItem->dtype == CFG_DTYPE_FILE) {
      free(pItem->fileVal);
    } else {
    }
    free(pItem->name);
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }

  return 0;
}

int32_t cfgAddBool(SConfig *pConfig, const char *name, bool defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_BOOL, .boolVal = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddInt8(SConfig *pConfig, const char *name, int8_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_INT8, .int8Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddUInt8(SConfig *pConfig, const char *name, uint8_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_UINT8, .uint8Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddInt16(SConfig *pConfig, const char *name, int16_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_INT16, .int16Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddUInt16(SConfig *pConfig, const char *name, uint16_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_UINT16, .uint16Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddInt32(SConfig *pConfig, const char *name, int32_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_INT32, .int32Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddUInt32(SConfig *pConfig, const char *name, uint32_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_UINT32, .uint32Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddInt64(SConfig *pConfig, const char *name, int64_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_INT64, .int64Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddUInt64(SConfig *pConfig, const char *name, uint64_t defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_UINT64, .uint64Val = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddFloat(SConfig *pConfig, const char *name, float defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_FLOAT, .floatVal = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddDouble(SConfig *pConfig, const char *name, double defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_DOUBLE, .doubleVal = defaultVal};
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddString(SConfig *pConfig, const char *name, const char *defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_STRING};
  item.strVal = strdup(defaultVal);
  if (item.strVal == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddFqdn(SConfig *pConfig, const char *name, const char *defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_FQDN};
  item.fqdnVal = strdup(defaultVal);
  if (item.fqdnVal == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddIpStr(SConfig *pConfig, const char *name, const char *defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_IPSTR};
  item.ipstrVal = strdup(defaultVal);
  if (item.ipstrVal == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddDir(SConfig *pConfig, const char *name, const char *defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_DIR};
  item.dirVal = strdup(defaultVal);
  if (item.dirVal == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  return cfgAddItem(pConfig, &item, name, utype);
}

int32_t cfgAddFile(SConfig *pConfig, const char *name, const char *defaultVal, ECfgUnitType utype) {
  SConfigItem item = {.dtype = CFG_DTYPE_FILE};
  item.fileVal = strdup(defaultVal);
  if (item.fileVal == NULL) {
    terrno = TSDB_CODE_OUT_OF_MEMORY;
    return -1;
  }
  return cfgAddItem(pConfig, &item, name, utype);
}

const char *cfgStypeStr(ECfgSrcType type) {
  switch (type) {
    case CFG_TYPE_DEFAULT:
      return "default";
    case CFG_TYPE_CFG_FILE:
      return "cfg_file";
    case CFG_TYPE_ENV_FILE:
      return "env_file";
    case CFG_TYPE_ENV_VAR:
      return "env_var";
    case CFG_TYPE_APOLLO_URL:
      return "apollo_url";
    default:
      return "invalid";
  }
}

const char *cfgDtypeStr(ECfgDataType type) {
  switch (type) {
    case CFG_DTYPE_NONE:
      return "none";
    case CFG_DTYPE_BOOL:
      return "bool";
    case CFG_DTYPE_INT8:
      return "int8";
    case CFG_DTYPE_UINT8:
      return "uint8";
    case CFG_DTYPE_INT16:
      return "int16";
    case CFG_DTYPE_UINT16:
      return "uint16";
    case CFG_DTYPE_INT32:
      return "int32";
    case CFG_DTYPE_UINT32:
      return "uint32";
    case CFG_DTYPE_INT64:
      return "int64";
    case CFG_DTYPE_UINT64:
      return "uint64";
    case CFG_DTYPE_FLOAT:
      return "float";
    case CFG_DTYPE_DOUBLE:
      return "double";
    case CFG_DTYPE_STRING:
      return "string";
    case CFG_DTYPE_FQDN:
      return "fqdn";
    case CFG_DTYPE_IPSTR:
      return "ipstr";
    case CFG_DTYPE_DIR:
      return "dir";
    case CFG_DTYPE_FILE:
      return "file";
    default:
      return "invalid";
  }
}

const char *cfgUtypeStr(ECfgUnitType type) {
  switch (type) {
    case CFG_UTYPE_NONE:
      return "";
    case CFG_UTYPE_PERCENT:
      return "(%)";
    case CFG_UTYPE_GB:
      return "(GB)";
    case CFG_UTYPE_MB:
      return "(Mb)";
    case CFG_UTYPE_BYTE:
      return "(byte)";
    case CFG_UTYPE_SECOND:
      return "(s)";
    case CFG_UTYPE_MS:
      return "(ms)";
    default:
      return "invalid";
  }
}