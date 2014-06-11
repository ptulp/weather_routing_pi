/***************************************************************************
 *
 * Project:  OpenCPN Weather Routing plugin
 * Author:   Sean D'Epagnier
 *
 ***************************************************************************
 *   Copyright (C) 2014 by Sean D'Epagnier                                 *
 *   sean@depagnier.com                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,  USA.         *
 ***************************************************************************
 *
 */

#include <wx/wx.h>

#include <map>

#include <stdlib.h>
#include <math.h>
#include <time.h>

#include "ocpn_plugin.h"

#include "Utilities.h"
#include "Boat.h"
#include "RouteMapOverlay.h"

#include "WeatherRouting.h"

ReportDialog::ReportDialog( WeatherRouting &weatherrouting )
    : ReportDialogBase(&weatherrouting), m_WeatherRouting(weatherrouting)
{
    SetRouteMapOverlay(NULL);
}

void ReportDialog::SetRouteMapOverlay(RouteMapOverlay *routemapoverlay)
{
    GenerateRoutesReport();

    if(routemapoverlay == NULL) {
        m_htmlConfigurationReport->SetPage(_("No Configuration selected."));
        return;
    }

    if(!routemapoverlay->ReachedDestination()) {
        m_htmlConfigurationReport->SetPage(_("Destination not yet reached."));
        return;
    }

    wxString page;
    RouteMapConfiguration c = routemapoverlay->GetConfiguration();
    std::list<PlotData> p = routemapoverlay->GetPlotData();

    page += _("Route from ") + c.Start + _(" to ") + c.End + _T("<dt>");
    page += _("Leaving ") + c.StartTime.Format() + _T("<dt>");
    page += _("Arriving ") + routemapoverlay->EndTime().Format() + _T("<dt>");
    page += _("Duration ") + (routemapoverlay->EndTime() - c.StartTime).Format() + _T("<dt>");
    page += _T("<p>");
    double distance = DistGreatCircle_Plugin(c.StartLat, c.StartLon, c.EndLat, c.EndLon);
    double distance_sailed = routemapoverlay->RouteInfo(RouteMapOverlay::DISTANCE);
    page += _("Distance sailed: ") + wxString::Format
        (_T("%.2f NMi : %.2f NMi or %.2f%% "), distance_sailed,
         distance_sailed - distance, (distance_sailed / distance - 1) * 100.0) +
        _("longer than great circle route") + _T("<br>");

    double avgspeed = routemapoverlay->RouteInfo(RouteMapOverlay::AVGSPEED);
    double avgspeedwater = routemapoverlay->RouteInfo(RouteMapOverlay::AVGSPEEDWATER);
    page += _("Average Ground Speed") + _T(": ") + wxString::Format
        (_T(" %.2f"), avgspeed) + _T(" knots<dt>");
    page += _("Average Water Speed") + _T(": ") + wxString::Format
        (_T(" %.2f"), avgspeedwater) + _T(" knots<dt>");
    page += _("Average Wind") + _T(": ") + wxString::Format
        (_T(" %.2f"), routemapoverlay->RouteInfo(RouteMapOverlay::AVGWIND)) + _T(" knots<dt>");
    page += _("Average Wave ht") + _T(": ") + wxString::Format
        (_T(" %.2f"), routemapoverlay->RouteInfo(RouteMapOverlay::AVGWAVE)) + _T(" meters<dt>");
    page += _("Upwind") + _T(": ") + wxString::Format
        (_T(" %.2f%%"), routemapoverlay->RouteInfo(RouteMapOverlay::PERCENTAGE_UPWIND)) + _T("<dt>");
    double port_starboard = routemapoverlay->RouteInfo(RouteMapOverlay::PORT_STARBOARD);
    page += _("Port/Starboard") + _T(": ") +
        (isnan(port_starboard) ? _T("nan") : wxString::Format
         (_T("%d/%d"), (int)port_starboard, 100-(int)port_starboard)) + _T("<dt>");

    Position *destination = routemapoverlay->GetDestination();

    page += _("Number of tacks") + wxString::Format(_T(": %d "), destination->tacks) + _T("<dt>\n");

    /* determine if currents significantly improve this (boat over ground speed average is 10% or
       more faster than boat over water)  then attempt to determine which current based on lat/lon
       eg, gulf stream, japan, current aghulles current etc.. and report it. */
    page += _T("<p>");
    double wspddiff = avgspeed / avgspeedwater;
    if(fabs(1-wspddiff) > .03) {
        page += wxString::Format
            (_T("%.2f%% "), ((wspddiff > 1 ?
                              avgspeed / avgspeedwater : avgspeedwater / avgspeed) - 1) * 100.0)
            + _("speed change due to ");
        if(wspddiff > 1)
            page += _("favorable");
        else
            page += _("unfavorable");
        page += _(" currents.");
    }

    m_htmlConfigurationReport->SetPage(page);
}

void ReportDialog::GenerateRoutesReport()
{
    /* sort configurations interate over each group of configurations
       with the same start and end to determine best and worst times,
       and cyclone crossings to determine cyclone times
    */

    std::map<wxString, std::list<RouteMapOverlay *> > routes;
    for(std::list<WeatherRoute*>::iterator it = m_WeatherRouting.m_WeatherRoutes.begin();
        it != m_WeatherRouting.m_WeatherRoutes.end(); it++) {
        if(!(*it)->routemapoverlay->ReachedDestination())
            continue;
        wxString route_string = (*it)->Start + _T(" - ") + (*it)->End;
        std::list<RouteMapOverlay *> overlays = routes[route_string];
        overlays.push_back((*it)->routemapoverlay);
        routes[route_string] = overlays;
    }

    if(routes.size() == 0) {
        m_htmlRoutesReport->SetPage(_("No routes to report yet."));
        return;
    }

    wxString page;
    for(std::map<wxString, std::list<RouteMapOverlay *> >::iterator it = routes.begin();
        it != routes.end(); it++) {
        std::list<RouteMapOverlay *> overlays = it->second;
        RouteMapOverlay *first = *overlays.begin();
        RouteMapConfiguration c = first->GetConfiguration();
        page += _T("<p>");
        page += c.Start + _(" to ") + c.End + _T(" ") + wxString::Format
            (_("(%ld configurations)\n"), overlays.size());

        /* determine fastest time */
        wxTimeSpan fastest_time;
        RouteMapOverlay *fastest;
        for(std::list<RouteMapOverlay *>::iterator it2 = overlays.begin(); it2 != overlays.end(); it2++) {
            wxTimeSpan current_time = ((*it2)->EndTime() - (*it2)->StartTime());
            if(*it2 == first || current_time < fastest_time) {
                fastest_time = current_time;
                fastest = *it2;
            }
        }
        page += _("<dt>Fastest configuration leaves ") + fastest->StartTime().Format();
        page += _("<dt>average speed") + wxString::Format
            (_T(": %.2f knots"), fastest->RouteInfo(RouteMapOverlay::AVGSPEED));

        /* determine best times if upwind percentage is below 50 */
        page += _T("<dt>");
        page += _("Best Times (mostly downwind)") + _T(": ");

        wxDateTime first_cyclone_time, last_cyclone_time;
        bool last_bad, any_bad, any_good = false, first_print = true;

        wxDateTime best_time_start;

        std::list<RouteMapOverlay *>::iterator it2, it2end = overlays.begin(), prev;
        last_bad = overlays.back()->RouteInfo(RouteMapOverlay::PERCENTAGE_UPWIND) > 50;

        for(it2 = overlays.begin(); it2 != overlays.end(); it2++)
            if((*it2)->RouteInfo(RouteMapOverlay::PERCENTAGE_UPWIND) > 50) {
                any_bad = last_bad = true;
            } else {
                if(!best_time_start.IsValid() && last_bad) {
                    best_time_start = (*it2)->StartTime();
                    it2end = it2;
                    it2++;
                    break;
                }
                last_bad = false;
            }

        if(it2 == overlays.end())
            it2++;
        for(;;) {
            if(it2 == it2end)
                break;
            it2++;
            if(it2 == overlays.end())
                it2++;

            if((*it2)->RouteInfo(RouteMapOverlay::PERCENTAGE_UPWIND) > 50) {
                if(!last_bad) {
                    prev = it2;
                    prev--;
                    if(prev == overlays.end()) prev--;
                    page += best_time_start.Format(_T("%d %B ")) +
                        _("to") + (*prev)->EndTime().Format(_T(" %d %B"));
                    if(first_print)
                        first_print = false;
                    else
                        page += _(" and ");
                }
                last_bad = any_bad = true;
            } else {
                if(last_bad)
                    best_time_start = (*it2)->StartTime();
                last_bad = false;
                any_good = true;
            }
/* disabled until we fix this, because it is really slow
            wxDateTime first, last;
            if((*it2)->CycloneTimes(first, last)) {
                if(!first_cyclone_time.IsValid() || first < first_cyclone_time)
                    first_cyclone_time = (*it2)->StartTime();
                if(!last_cyclone_time.IsValid() || last < last_cyclone_time)
                    last_cyclone_time = (*it2)->EndTime();
            }
*/
        }

        if(!any_bad)
            page += _("any");
        else if(!any_good)
            page += _("none");

        page += _T("<dt>");
        page += _("Cyclones") + _T(": ");
        if(first_cyclone_time.IsValid())
            page += wxDateTime::GetMonthName(first_cyclone_time.GetMonth()) + _(" to ") +
                    wxDateTime::GetMonthName(first_cyclone_time.GetMonth());
        else if(RouteMap::ClimatologyCycloneTrackCrossings)
            page += _("none");
        else
            page += _("unavailable");
    }

    m_htmlRoutesReport->SetPage(page);
}

void ReportDialog::OnInformation( wxCommandEvent& event )
{
    wxMessageDialog mdlg(this, _("\
Weather Routing Reports gives an overview of a given route based on multiple configurations.\n\n\
For example using the configuration batch dialog, it is possible to easily generate multiple \
otherwise identical configurations which have different starting times. \
Once all of these configurations are computed, they become available to the report generator. \
An overview can be given of the best times, expected speed, and weather conditions. \
If climatology is available, cyclone risk and additional weather conditions may be described."),
                         _("Weather Routing Report"), wxOK | wxICON_INFORMATION);
    mdlg.ShowModal();
}
