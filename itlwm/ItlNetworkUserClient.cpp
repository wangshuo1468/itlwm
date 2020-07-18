/*
* Copyright (C) 2020  钟先耀
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 2 of the License, or
* (at your option) any later version.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/

#include "ItlNetworkUserClient.hpp"

#define super IOUserClient
OSDefineMetaClassAndStructors( ItlNetworkUserClient, IOUserClient );

const IOControlMethodAction ItlNetworkUserClient::sMethods[IOCTL_ID_MAX] {
    sDRIVER_INFO,
    sSTA_INFO,
    sPOWER,
    sSTATE,
    sNW_ID,
    sWPA_KEY,
    sASSOCIATE,
    sDISASSOCIATE,
    sJOIN,
    sSCAN,
    sSCAN_RESULT,
    sTX_POWER_LEVEL,
};

bool ItlNetworkUserClient::initWithTask(task_t owningTask, void *securityID, UInt32 type, OSDictionary *properties)
{
    fTask = owningTask;
    return super::initWithTask(owningTask, securityID, type, properties);
}

bool ItlNetworkUserClient::start(IOService *provider)
{
//    IOLog("start\n");
    if( !super::start( provider ))
        return false;
    fDriver = OSDynamicCast(itlwm, provider);
    fInf = fDriver->getNetworkInterface();
    fIfp = fDriver->getIfp();
    fSoft = fDriver->getSoft();
    if (fDriver == NULL) {
        return false;
    }
    return true;
}

IOReturn ItlNetworkUserClient::clientClose()
{
//    IOLog("clientClose\n");
    if( !isInactive())
        terminate();
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::clientDied()
{
//    IOLog("clientDied\n");
    return super::clientDied();
}

void ItlNetworkUserClient::stop(IOService *provider)
{
//    IOLog("stop\n");
    super::stop( provider );
}

IOReturn ItlNetworkUserClient::externalMethod(uint32_t selector, IOExternalMethodArguments * arguments, IOExternalMethodDispatch * dispatch, OSObject * target, void * reference)
{
    bool isSet = selector & IOCTL_MASK;
    selector &= ~IOCTL_MASK;
//    IOLog("externalMethod invoke. selector=0x%X isSet=%d\n", selector, isSet);
    if (selector < 0 || selector > IOCTL_ID_MAX) {
        return super::externalMethod(selector, arguments, NULL, this, NULL);
    }
    void *data = isSet ? (void *)arguments->structureInput : (void *)arguments->structureOutput;
    if (!data) {
        return kIOReturnError;
    }
    return sMethods[selector](this, data, isSet);
}

IOReturn ItlNetworkUserClient::
sDRIVER_INFO(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    ioctl_driver_info *drv_info = (ioctl_driver_info *)data;
    memset(drv_info, 0, sizeof(*drv_info));
    drv_info->version = IOCTL_VERSION;
    snprintf(drv_info->bsd_name, sizeof(drv_info->bsd_name), "%s%d", ifnet_name(that->fInf->getIfnet()), ifnet_unit(that->fInf->getIfnet()));
    strncpy(drv_info->fw_version, that->fSoft->sc_fwver, sizeof(drv_info->fw_version));
    memcpy(drv_info->driver_version, "1.0.0d", sizeof(drv_info->driver_version));
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sSTA_INFO(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_sta_info *st = (struct ioctl_sta_info *)data;
    struct ieee80211com *ic = &that->fSoft->sc_ic;
    struct ieee80211_node *ic_bss = that->fSoft->sc_ic.ic_bss;
    if (isSet) {
        return kIOReturnError;
    }
    if (ic_bss == NULL) {
        return kIOReturnError;
    }
    if (ic_bss->ni_chan == NULL) {
        return kIOReturnError;
    }
    st->version = IOCTL_VERSION;
    st->op_mode = ITL80211_MODE_11N;
    st->max_mcs = ic_bss->ni_txmcs;
    st->cur_mcs = ic_bss->ni_txmcs;
    st->channel = ieee80211_chan2ieee(&that->fSoft->sc_ic, ic_bss->ni_chan);
    //TODO only support 20mhz band width now
    st->band_width = 20;
    st->rssi = -(0 - IWM_MIN_DBM - ic_bss->ni_rssi);
    st->noise = 0;
    st->rate = ic_bss->ni_rates.rs_rates[ic_bss->ni_txrate];
    memset(st->ssid, 0, sizeof(st->ssid));
    bcopy(ic->ic_des_essid, st->ssid, ic->ic_des_esslen);
    memset(st->bssid, 0, sizeof(st->bssid));
    bcopy(ic->ic_bss->ni_bssid, st->bssid, ETHER_ADDR_LEN);
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sPOWER(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_power *ip = (struct ioctl_power *)data;
    if (isSet) {
        if (ip->enabled) {
            that->fDriver->enable(that->fInf);
        } else {
            that->fDriver->disable(that->fInf);
        }
    } else {
        memset(ip, 0, sizeof(*ip));
        ip->version = IOCTL_VERSION;
        ip->enabled = (that->fIfp->if_flags & (IFF_UP | IFF_RUNNING)) ==
        (IFF_UP | IFF_RUNNING) ? 1 : 0;
    }
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sSTATE(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_state *st = (struct ioctl_state *)data;
    if (isSet) {
        return kIOReturnError;
    }
    memset(st, 0, sizeof(*st));
    st->version = IOCTL_VERSION;
    st->state = (itl_80211_state)that->fSoft->sc_ic.ic_state;
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sNW_ID(OSObject* target, void* data, bool isSet)
{
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sWPA_KEY(OSObject* target, void* data, bool isSet)
{
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sASSOCIATE(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_associate *as = (struct ioctl_associate *)data;
    that->fDriver->associateSSID(as->nwid.nwid, as->wpa_key.key);
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sDISASSOCIATE(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_disassociate *dis = (struct ioctl_disassociate *)data;
    struct ieee80211com *ic = &that->fSoft->sc_ic;
    that->fDriver->iwm_stop(&ic->ic_ac.ac_if);
    ieee80211_del_ess(ic, (char *)dis->ssid, strlen((char *)dis->ssid), 0);
    ieee80211_deselect_ess(&that->fSoft->sc_ic);
    that->fDriver->iwm_add_task(that->fSoft, systq, &that->fSoft->init_task);
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sJOIN(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_join *join = (struct ioctl_join *)data;
    that->fDriver->joinSSID(join->nwid.nwid, join->wpa_key.key);
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sSCAN(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ieee80211com *ic = &that->fSoft->sc_ic;
    if (that->fSoft->sc_ic.ic_state == IEEE80211_S_RUN) {
        //TODO need to scan all of the channels.
//        timeout_add_usec(&ic->ic_bgscan_timeout, 1);
    }
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sSCAN_RESULT(OSObject* target, void* data, bool isSet)
{
    ItlNetworkUserClient *that = OSDynamicCast(ItlNetworkUserClient, target);
    struct ioctl_network_info *ni = (struct ioctl_network_info *)data;
    ieee80211com *ic = &that->fSoft->sc_ic;
    if (that->fNextNodeToSend == NULL) {
        if (that->fScanResultWrapping) {
            that->fScanResultWrapping = false;
            return kIONoScanResult;
        } else {
            that->fNextNodeToSend = RB_MIN(ieee80211_tree, &ic->ic_tree);
            if (that->fNextNodeToSend == NULL) {
                return kIONoScanResult;
            }
        }
    }
    bzero(ni, sizeof(*ni));
    
    ni->ni_rsncaps = that->fNextNodeToSend->ni_capinfo;;
    ni->channel = ieee80211_chan2ieee(&that->fSoft->sc_ic, that->fNextNodeToSend->ni_chan);
    ni->ni_rsncipher = (enum itl80211_cipher)that->fNextNodeToSend->ni_rsncipher;
    ni->rsn_akms = that->fNextNodeToSend->ni_rsnakms;
    ni->rsn_ciphers = that->fNextNodeToSend->ni_rsnciphers;
    ni->rsn_protos = that->fNextNodeToSend->ni_rsnprotos;
    ni->rsn_groupcipher = (enum itl80211_cipher)that->fNextNodeToSend->ni_rsngroupcipher;
    ni->rsn_groupmgmtcipher = (enum itl80211_cipher)that->fNextNodeToSend->ni_rsngroupmgmtcipher;
    ni->supported_rsnakms = that->fNextNodeToSend->ni_supported_rsnakms;
    ni->supported_rsnprotos = that->fNextNodeToSend->ni_supported_rsnprotos;
    ni->noise = 0;
    ni->rssi = -(0 - IWM_MIN_DBM - that->fNextNodeToSend->ni_rssi);
    memcpy(ni->bssid, that->fNextNodeToSend->ni_bssid, 6);
    memcpy(ni->ssid, that->fNextNodeToSend->ni_essid, 32);
    that->fNextNodeToSend = RB_NEXT(ieee80211_tree, &ic->ic_tree, that->fNextNodeToSend);
    if (that->fNextNodeToSend == NULL)
        that->fScanResultWrapping = true;
    return kIOReturnSuccess;
}

IOReturn ItlNetworkUserClient::
sTX_POWER_LEVEL(OSObject* target, void* data, bool isSet)
{
    return kIOReturnSuccess;
}
