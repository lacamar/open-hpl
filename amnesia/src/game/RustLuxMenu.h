#ifndef LUX_RUST_MENU_H
#define LUX_RUST_MENU_H

// C ABI of rust/src/luxmenu/ffi.rs: profile list rules of the profile window.

#include <cstddef>

struct OhplGa7aDate
{
	int seconds, minutes, hours, month_day, month, year;
};

extern "C"
{
void ohpl_ga7a_pf_sort(const OhplGa7aDate *apDates, size_t alNum, unsigned *apOut);
void ohpl_ga7a_pf_trim(const unsigned *apName, size_t alLen, size_t *apStart, size_t *apEnd);
const unsigned *ohpl_ga7a_pf_illegal(size_t *apLen);
int ohpl_ga7a_pf_name_max();
int ohpl_ga7a_pf_select(int alNum, size_t alCount);
int ohpl_ga7a_pf_delete_plan(int alSelected, int abInUse, int abConfigDiffers, int *apNewSelection);
int ohpl_ga7a_pf_activate(int alItemNum);
int ohpl_ga7a_pf_ui_press(int abSecondary, int abListLocked, int abNameVisible);
}

#endif // LUX_RUST_MENU_H
