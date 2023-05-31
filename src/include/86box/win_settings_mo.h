/*
 * VARCem	Virtual ARchaeological Computer EMulator.
 *		An emulator of (mostly) x86-based PC systems and devices,
 *		using the ISA,EISA,VLB,MCA  and PCI system buses, roughly
 *		spanning the era between 1981 and 1995.
 *
 *		This file is part of the VARCem Project.
 *
 *		Implementation of the "Magneto-optical Devices" dialog.
 *
 * Version:	@(#)win_settings_mo.h	1.0.2	2021/04/09
 *
 * Authors:     Natalia Portillo, <claunia@claunia.com>
 *              Fred N. van Kempen, <decwiz@yahoo.com>
 *              Altheos, <altheos@varcem.com>
 *              Miran Grca, <mgrca8@gmail.com>
 *
 *		Copyright 2020      Natalia Portillo.
 *		Copyright 2017-2021 Fred N. van Kempen.
 *		Copyright 2021      Altheos.
 *		Copyright 2016-2018 Miran Grca.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free  Software  Foundation; either  version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is  distributed in the hope that it will be useful, but
 * WITHOUT   ANY  WARRANTY;  without  even   the  implied  warranty  of
 * MERCHANTABILITY  or FITNESS  FOR A PARTICULAR  PURPOSE. See  the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the:
 *
 *   Free Software Foundation, Inc.
 *   59 Temple Place - Suite 330
 *   Boston, MA 02111-1307
 *   USA.
 */

/************************************************************************
  *									*
  *			  Magento-Optical Devices Dialog			*
  *									*
  ************************************************************************/

/* GLobal variables needed for the MO Devices dialog. */

static int mo_ignore_change = 0;
static int modlv_current_sel;
static int listtomodel[64];

static int
mo_combo_to_string(int id)
{
    if (id == 0)
    	return(IDS_DISABLED);

    return(IDS_3580 + id - 1);
}

static void
mo_id_to_combo(HWND h, int num)
{
    WCHAR temp[16];
    int i;

    for (i = 0; i < num; i++) {
    	swprintf(temp, sizeof_w(temp), L"%i", i);
    	SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}

static void
mo_track_init(void) {
    int i;

    for (i = 0; i < MO_NUM; i++) {
        if (mo_drives[i].bus_type == MO_BUS_ATAPI)
            ide_tracking |= (4 << (mo_drives[i].bus_id.ide_channel << 3));
        else if (mo_drives[i].bus_type == MO_BUS_SCSI)
            scsi_tracking[mo_drives[i].bus_id.scsi.id] |= (4 << (mo_drives[i].bus_id.scsi.lun << 3));
    }
}

static void
mo_image_list_init(HWND hwndList) {
    HICON      hiconItem;
    HIMAGELIST hSmall;

    hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
                              GetSystemMetrics(SM_CYSMICON),
                              ILC_MASK | ILC_COLOR32, 1, 1);

    hiconItem = LoadIcon(hInstance, (LPCWSTR)ICON_MO_D);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    hiconItem = LoadIcon(hInstance, (LPCWSTR)ICON_MO);
    ImageList_AddIcon(hSmall, hiconItem);
    DestroyIcon(hiconItem);

    ListView_SetImageList(hwndList, hSmall, LVSIL_SMALL);
}

static BOOL
mo_recalc_list(HWND hwndList) {
    WCHAR  temp[256];
    LVITEM lvI;
    int    fsid, i;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    for (i = 0; i < MO_NUM; i++) {
        fsid = temp_mo_drives[i].bus_type;

        lvI.iSubItem = 0;
        switch (fsid) {
            case MO_BUS_DISABLED:
            default:
                lvI.pszText = (LPTSTR)get_string(IDS_DISABLED);
                lvI.iImage = 0;
                break;

            case MO_BUS_ATAPI:
                swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
                         get_string(mo_combo_to_string(fsid)),
                         temp_mo_drives[i].bus_id.ide_channel >> 1,
                         temp_mo_drives[i].bus_id.ide_channel & 1);
                lvI.pszText = temp;
                lvI.iImage = 1;
                break;

            case MO_BUS_SCSI:
                swprintf(temp, sizeof_w(temp), L"%ls (%02i:%02i)",
                         get_string(mo_combo_to_string(fsid)),
                         temp_mo_drives[i].bus_id.scsi.id,
                         temp_mo_drives[i].bus_id.scsi.lun);
                lvI.pszText = temp;
                lvI.iImage = 1;
                break;
        }
        lvI.iItem = i;
        if (ListView_InsertItem(hwndList, &lvI) == -1)
            return FALSE;
    
    	lvI.iSubItem = 1;
		if (fsid == MO_BUS_DISABLED)
            swprintf(temp, sizeof_w(temp), L"%s", " ");
		else
            swprintf(temp, sizeof_w(temp), L"%s", mo_get_internal_name((temp_mo_drives[i].type)));
    	lvI.pszText = temp;
    	
    lvI.iItem = i;
    	if (ListView_SetItem(hwndList, &lvI) == -1)
    		return FALSE;
	}

    return TRUE;
}

static BOOL
mo_init_columns(HWND hwndList) {
    LVCOLUMN lvc;

    lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;

    lvc.iSubItem = 0;
    lvc.pszText = (LPTSTR)get_string(IDS_BUS);
    lvc.cx = 100;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 0, &lvc) == -1)
        return FALSE;

    lvc.iSubItem = 1;
    lvc.pszText = (LPTSTR)get_string(IDS_TYPE);
    lvc.cx = 250;
    lvc.fmt = LVCFMT_LEFT;
    if (ListView_InsertColumn(hwndList, 1, &lvc) == -1)
        return FALSE;

    return TRUE;
}

static int
mo_get_selected(HWND hdlg) {
    int  drive = -1;
    int  i, j = 0;
    HWND h;

    for (i = 0; i < MO_NUM; i++) {
        h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
        j = ListView_GetItemState(h, i, LVIS_SELECTED);
        if (j)
            drive = i;
    }

    return drive;
}

static void
mo_update_item(HWND hwndList, int i) {
    WCHAR  temp[128];
    LVITEM lvI;
    int    fsid;

    lvI.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE;
    lvI.stateMask = lvI.iSubItem = lvI.state = 0;

    lvI.iItem = i;

    fsid = temp_mo_drives[i].bus_type;

   	switch (fsid) {
      	case MO_BUS_DISABLED:
       	default:
           	lvI.pszText = (LPTSTR)get_string(IDS_DISABLED);
           	lvI.iImage = 0;
           	break;

       	case MO_BUS_ATAPI:
           	swprintf(temp, sizeof_w(temp), L"%ls (%01i:%01i)",
                    	get_string(combo_to_string(fsid)),
                    	temp_mo_drives[i].bus_id.ide_channel >> 1,
                    	temp_mo_drives[i].bus_id.ide_channel & 1);
           	lvI.pszText = temp;
           	lvI.iImage = 1;
           	break;

       	case MO_BUS_SCSI:
           	swprintf(temp, sizeof_w(temp), L"%ls (%02i:%02i)",
                    	get_string(combo_to_string(fsid)),
                    	temp_mo_drives[i].bus_id.scsi.id,
                    	temp_mo_drives[i].bus_id.scsi.lun);
            	lvI.pszText = temp;
            	lvI.iImage = 1;
            	break;
    	}
    if (ListView_SetItem(hwndList, &lvI) == -1)
        return;
    
    lvI.iSubItem = 1;
	if (fsid == MO_BUS_DISABLED)
        swprintf(temp, sizeof_w(temp), L"%s", " ");
	else
        swprintf(temp, sizeof_w(temp), L"%s", mo_get_internal_name((temp_mo_drives[i].type)));
	lvI.pszText = temp;

    lvI.iItem = i;
    if (ListView_SetItem(hwndList, &lvI) == -1)
    	return;
}

static void
mo_add_locations(HWND hdlg) {
    WCHAR temp[128];
    HWND  h;
    int   i;

    h = GetDlgItem(hdlg, IDC_COMBO_MO_BUS);
    for (i = MO_BUS_DISABLED; i < MO_BUS_MAX; i++)
        SendMessage(h, CB_ADDSTRING, 0, win_string(combo_to_string(i)));

    h = GetDlgItem(hdlg, IDC_COMBO_MO_ID);
    mo_id_to_combo(h, 16);

    h = GetDlgItem(hdlg, IDC_COMBO_MO_LUN);
    mo_id_to_combo(h, 8);

    h = GetDlgItem(hdlg, IDC_COMBO_MO_CHANNEL_IDE);
    for (i = 0; i < 8; i++) {
        swprintf(temp, sizeof_w(temp), L"%01i:%01i", i >> 1, i & 1);
        SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
    
    h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
	for (i = 0; i < (KNOWN_MO_VENDORS); i++) {
		swprintf(temp, sizeof_w(temp), L"%s",mo_get_internal_name(i));
		SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
    }
}

static void
mo_recalc_model_locations(HWND hdlg) {
    WCHAR temp[128];
    HWND  h;
    int   i, j;
    int   b;

    h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
    SendMessage(h, CB_RESETCONTENT, 0, 0);
    b = temp_mo_drives[modlv_current_sel].bus_type - 1;
    i = j = 0;
    memset(listtomodel, 0x00, sizeof(listtomodel));

    while (i < KNOWN_MO_VENDORS) {
		if (mo_drive_types[i].supported_bus[b]) {
			swprintf(temp, sizeof_w(temp), L"%s",mo_get_internal_name(i));
			SendMessage(h, CB_ADDSTRING, 0, (LPARAM)temp);
           	listtomodel[j] = i;
			if (i == temp_mo_drives[modlv_current_sel].type)
				SendMessage(h, CB_SETCURSEL, j, 0);
           	j++;
		}
    	i++;
    }

}

static void
mo_recalc_location_controls(HWND hdlg, int assign_id) {
    HWND h;
    int  i;
    int  bus = temp_mo_drives[modlv_current_sel].bus_type;

    for (i = IDT_1754; i < (IDT_1756 + 1); i++) {
        h = GetDlgItem(hdlg, i);
        EnableWindow(h, FALSE);
        ShowWindow(h, SW_HIDE);
    }

    h = GetDlgItem(hdlg, IDC_COMBO_MO_ID);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_MO_LUN);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDC_COMBO_MO_CHANNEL_IDE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    h = GetDlgItem(hdlg, IDT_1781);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);    
    h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
    EnableWindow(h, FALSE);
    ShowWindow(h, SW_HIDE);

    switch (bus) {
        case MO_BUS_ATAPI:
            h = GetDlgItem(hdlg, IDT_1756);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);

            if (assign_id)
                temp_mo_drives[modlv_current_sel].bus_id.ide_channel = next_free_ide_channel();

            h = GetDlgItem(hdlg, IDC_COMBO_MO_CHANNEL_IDE);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            SendMessage(h, CB_SETCURSEL,
                        temp_mo_drives[modlv_current_sel].bus_id.ide_channel, 0);
			h = GetDlgItem(hdlg, IDT_1781);
    		EnableWindow(h, TRUE);
    		ShowWindow(h, SW_SHOW); 
    		h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
    		ShowWindow(h, SW_SHOW);
    		EnableWindow(h, TRUE);
            mo_recalc_model_locations(hdlg);     
            break;

        case MO_BUS_SCSI:
            h = GetDlgItem(hdlg, IDT_1754);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            h = GetDlgItem(hdlg, IDT_1755);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            if (assign_id)
                next_free_scsi_id_and_lun((uint8_t *)&temp_mo_drives[modlv_current_sel].bus_id.scsi.id, (uint8_t *)&temp_mo_drives[modlv_current_sel].bus_id.scsi.lun);

            h = GetDlgItem(hdlg, IDC_COMBO_MO_ID);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            SendMessage(h, CB_SETCURSEL,
                        temp_mo_drives[modlv_current_sel].bus_id.scsi.id, 0);

            h = GetDlgItem(hdlg, IDC_COMBO_MO_LUN);
            ShowWindow(h, SW_SHOW);
            EnableWindow(h, TRUE);
            SendMessage(h, CB_SETCURSEL,
                        temp_mo_drives[modlv_current_sel].bus_id.scsi.lun, 0);
    		h = GetDlgItem(hdlg, IDT_1781);
    		EnableWindow(h, TRUE);
    		ShowWindow(h, SW_SHOW); 
    		h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
    		ShowWindow(h, SW_SHOW);
    		EnableWindow(h, TRUE);
    		mo_recalc_model_locations(hdlg);
            break;
    }
}

static void
mo_track(uint8_t id) {
    if (temp_mo_drives[id].bus_type == MO_BUS_ATAPI)
        ide_tracking |= (1ULL << temp_mo_drives[id].bus_id.ide_channel);
    else if (temp_mo_drives[id].bus_type == MO_BUS_SCSI)
        scsi_tracking[temp_mo_drives[id].bus_id.scsi.id] |= (1ULL << temp_mo_drives[id].bus_id.scsi.lun);
}

static void
mo_untrack(uint8_t id) {
    if (temp_mo_drives[id].bus_type == MO_BUS_ATAPI)
        ide_tracking &= ~(1ULL << temp_mo_drives[id].bus_id.ide_channel);
    else if (temp_mo_drives[id].bus_type == MO_BUS_SCSI)
        scsi_tracking[temp_mo_drives[id].bus_id.scsi.id] &= ~(1ULL << temp_mo_drives[id].bus_id.scsi.lun);
}

static WIN_RESULT CALLBACK
mo_devices_proc(HWND hdlg, UINT message, WPARAM wParam, LPARAM lParam) {
    HWND h = NULL;
    int  old_sel = 0;
    int  assign = 0;
    int  b;

    switch (message) {
        case WM_INITDIALOG:
            mo_ignore_change = 1;

            modlv_current_sel = 0;
            h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
            mo_init_columns(h);
            mo_image_list_init(h);
            mo_recalc_list(h);
            mo_add_locations(hdlg);
            h = GetDlgItem(hdlg, IDC_COMBO_MO_BUS);
            b = temp_mo_drives[modlv_current_sel].bus_type;
            SendMessage(h, CB_SETCURSEL, b, 0);
            mo_recalc_model_locations(hdlg);
            mo_recalc_location_controls(hdlg, 0);
            mo_ignore_change = 0;
            return TRUE;

        case WM_NOTIFY:
            if (mo_ignore_change)
                return FALSE;

            if ((((LPNMHDR)lParam)->code == LVN_ITEMCHANGED) && (((LPNMHDR)lParam)->idFrom == IDC_LIST_MO_DRIVES)) {
                old_sel = modlv_current_sel;
                h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
                modlv_current_sel = mo_get_selected(hdlg);
                if (modlv_current_sel == old_sel)
                    return FALSE;

                if (modlv_current_sel == -1) {
                    mo_ignore_change = 1;
                    modlv_current_sel = old_sel;
                    ListView_SetItemState(h, modlv_current_sel, LVIS_FOCUSED | LVIS_SELECTED, 0x000F);
                    mo_ignore_change = 0;
                    return FALSE;
                }
                mo_ignore_change = 1;

                h = GetDlgItem(hdlg, IDC_COMBO_MO_BUS);
                b = temp_mo_drives[modlv_current_sel].bus_type;
                SendMessage(h, CB_SETCURSEL, b, 0);
                h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
                b = temp_mo_drives[modlv_current_sel].type;
                SendMessage(h, CB_SETCURSEL, b, 0);
    			mo_recalc_location_controls(hdlg, 0);
            }
            mo_ignore_change = 0;
            break;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDC_COMBO_MO_BUS:
                    if (mo_ignore_change)
                        return FALSE;

                    mo_ignore_change = 1;
                    h = GetDlgItem(hdlg, IDC_COMBO_MO_BUS);
                    b = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
                    if (temp_mo_drives[modlv_current_sel].bus_type == b)
                        goto mo_bus_skip;
                    mo_untrack(modlv_current_sel);
                    assign = (temp_mo_drives[modlv_current_sel].bus_type == b) ? 0 : 1;
                    temp_mo_drives[modlv_current_sel].bus_type = b;
                    mo_recalc_location_controls(hdlg, assign);
                    mo_track(modlv_current_sel);
                    h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
                    mo_update_item(h, modlv_current_sel);
                mo_bus_skip:
                    mo_ignore_change = 0;
                    return FALSE;

                case IDC_COMBO_MO_ID:
                    if (mo_ignore_change)
                        return FALSE;

                    mo_ignore_change = 1;
                    h = GetDlgItem(hdlg, IDC_COMBO_MO_ID);
                    mo_untrack(modlv_current_sel);
                    b = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
                    temp_mo_drives[modlv_current_sel].bus_id.scsi.id = (uint8_t)b;
                    mo_track(modlv_current_sel);
                    h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
                    mo_update_item(h, modlv_current_sel);
                    mo_ignore_change = 0;
                    return FALSE;

                case IDC_COMBO_MO_LUN:
                    if (mo_ignore_change)
                        return FALSE;

                    mo_ignore_change = 1;
                    h = GetDlgItem(hdlg, IDC_COMBO_MO_LUN);
                    mo_untrack(modlv_current_sel);
                    b = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
                    temp_mo_drives[modlv_current_sel].bus_id.scsi.lun = (uint8_t)b;
                    mo_track(modlv_current_sel);
                    h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
                    mo_update_item(h, modlv_current_sel);
                    mo_ignore_change = 0;
                    return FALSE;

                case IDC_COMBO_MO_CHANNEL_IDE:
                    if (mo_ignore_change)
                        return FALSE;

                    mo_ignore_change = 1;
                    h = GetDlgItem(hdlg, IDC_COMBO_MO_CHANNEL_IDE);
                    mo_untrack(modlv_current_sel);
                    b = (int)SendMessage(h, CB_GETCURSEL, 0, 0);
                    temp_mo_drives[modlv_current_sel].bus_id.ide_channel = (uint8_t)b;
                    mo_track(modlv_current_sel);               
    				h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
                    mo_update_item(h, modlv_current_sel);
                    mo_ignore_change = 0;
                    return FALSE;

    			case IDC_COMBO_MO_TYPE:
    				if (HIWORD(wParam) == CBN_SELCHANGE) {
    					if (mo_ignore_change)
    							return FALSE;
    					mo_ignore_change = 1;
    					h = GetDlgItem(hdlg, IDC_COMBO_MO_TYPE);
    					temp_mo_drives[modlv_current_sel].type = listtomodel[(int)SendMessage(h, CB_GETCURSEL, 0, 0)];
    				}
    				h = GetDlgItem(hdlg, IDC_LIST_MO_DRIVES);
    				mo_update_item(h, modlv_current_sel);
    				mo_ignore_change = 0;
    			return FALSE;
            }
            return FALSE;

        default:
            break;
    }

    return FALSE;
}