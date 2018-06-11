/*  cdrdao - write audio CD-Rs in disc-at-once mode
 *
 *  Copyright (C) 1998-2000  Andreas Mueller <mueller@daneb.ping.de>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __TOC_EDIT_VIEW_H__
#define __TOC_EDIT_VIEW_H__

class TocEdit;

class TocEditView {
public:
    TocEditView(TocEdit *);
    ~TocEditView();

    TocEdit *tocEdit() const;

    void sampleMarker(unsigned long);
    bool sampleMarker(unsigned long *) const;

    void sampleSelect(unsigned long, unsigned long);
    void sampleSelectAll();
    bool sampleSelection(unsigned long *, unsigned long *) const;
    bool sampleSelectionClear();

    void sampleViewFull();
    void sampleView(unsigned long *, unsigned long *) const;
    bool sampleView(unsigned long smin, unsigned long smax);
    bool is_sample_view_full();

    void trackSelection(int);
    bool trackSelection(int *) const;

    void indexSelection(int);
    bool indexSelection(int *) const;
  
private:
    TocEdit *tocEdit_;

    bool sampleMarkerValid_;
    unsigned long sampleMarker_;

    bool sampleSelectionValid_;
    unsigned long sampleSelectionMin_;
    unsigned long sampleSelectionMax_;

    unsigned long sampleViewMin_;
    unsigned long sampleViewMax_;
  
    bool trackSelectionValid_;
    int trackSelection_;

    bool indexSelectionValid_;
    int indexSelection_;

};

#endif
