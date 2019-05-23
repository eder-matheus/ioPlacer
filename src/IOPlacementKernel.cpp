////////////////////////////////////////////////////////////////////////////////
// Authors: Vitor Bandeira, Mateus Fogaça, Eder Matheus Monteiro e Isadora
// Oliveira
//          (Advisor: Ricardo Reis)
//
// BSD 3-Clause License
//
// Copyright (c) 2019, Federal University of Rio Grande do Sul (UFRGS)
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
////////////////////////////////////////////////////////////////////////////////

/* TODO:  <15-05-19, the algorithm to assign an IOPin to a section does not
 * take into account if the net has N pins and N is greater than number of
 * slots in section > */

#define MAX_SLOTS_IN_SECTION 300
#define COST_MULT 1000

#include "IOPlacementKernel.h"
#include "Parser.h"
#include "WriterIOPins.h"

#include <random>

IOPlacementKernel::IOPlacementKernel(Parameters& parms) : _parms(&parms) {}

void IOPlacementKernel::randomPlacement(std::vector<IOPin>& assignment) {
        static const int kMaxValue = _slots.size() - 1;
        std::vector<int> v(kMaxValue + 1);
        for (size_t i = 0; i < v.size(); ++i) v[i] = i;
        std::shuffle(v.begin(), v.end(), std::default_random_engine(42));
        _netlist.forEachIOPin([&](unsigned idx, IOPin& ioPin) {
                unsigned b = v[0];
                ioPin.setPos(_slots.at(b).pos);
                assignment.push_back(ioPin);
                v.erase(v.begin());
        });
}

void IOPlacementKernel::initNetlistAndCore() {
        Parser parser(*_parms, _netlist, _core);
        _horizontalMetalLayer =
            "Metal" + std::to_string(_parms->getHorizontalMetalLayer());
        _verticalMetalLayer =
            "Metal" + std::to_string(_parms->getVerticalMetalLayer());
        parser.run();
}

void IOPlacementKernel::initIOLists() {
        _netlist.forEachIOPin([&](unsigned idx, IOPin& ioPin) {
                std::vector<InstancePin> instPinsVector;
                if (_netlist.numSinksOfIO(idx) != 0) {
                        _netlist.forEachSinkOfIO(
                            idx, [&](InstancePin& instPin) {
                                    instPinsVector.push_back(instPin);
                            });
                        _netlistIOPins.addIONet(ioPin, instPinsVector);
                } else {
                        _zeroSinkIOs.push_back(ioPin);
                }
        });
}

void IOPlacementKernel::defineSlots() {
        Coordinate lb = _core.getLowerBound();
        Coordinate ub = _core.getUpperBound();
        DBU lbX = lb.getX();
        DBU lbY = lb.getY();
        DBU ubX = ub.getX();
        DBU ubY = ub.getY();
        unsigned minDstPinsX = _core.getMinDstPinsX();
        unsigned minDstPinsY = _core.getMinDstPinsY();
        unsigned initTracksX = _core.getInitTracksX();
        unsigned initTracksY = _core.getInitTracksY();

        DBU totalNumSlots = 0;
        totalNumSlots += (ubX - lbX) * 2 / minDstPinsX;
        totalNumSlots += (ubY - lbY) * 2 / minDstPinsY;

        /*******************************************
         * How the for bellow follows core boundary *
         ********************************************
         *                 <----                    *
         *                                          *
         *                 3st edge     upperBound  *
         *           *------------------x           *
         *           |                  |           *
         *   |       |                  |      ^    *
         *   |  4th  |                  | 2nd  |    *
         *   |  edge |                  | edge |    *
         *   V       |                  |      |    *
         *           |                  |           *
         *           x------------------*           *
         *   lowerBound    1st edge                 *
         *                 ---->                    *
         *******************************************/

        std::vector<Coordinate> slotsEdge1;
        DBU currX = lb.getX() + initTracksX;
        DBU currY = lb.getY();

        while (currX < ub.getX()) {
                Coordinate pos(currX, currY);
                slotsEdge1.push_back(pos);
                currX += minDstPinsX;
        }

        std::vector<Coordinate> slotsEdge2;
        currY = lb.getY() + initTracksY;
        currX = ub.getX();
        while (currY < ub.getY()) {
                Coordinate pos(currX, currY);
                slotsEdge2.push_back(pos);
                currY += minDstPinsY;
        }

        std::vector<Coordinate> slotsEdge3;
        currX = lb.getX() + initTracksX;
        currY = ub.getY();
        while (currX < ub.getX()) {
                Coordinate pos(currX, currY);
                slotsEdge3.push_back(pos);
                currX += minDstPinsX;
        }
        std::reverse(slotsEdge3.begin(), slotsEdge3.end());

        std::vector<Coordinate> slotsEdge4;
        currY = lb.getY() + initTracksY;
        currX = lb.getX();
        while (currY < ub.getY()) {
                Coordinate pos(currX, currY);
                slotsEdge4.push_back(pos);
                currY += minDstPinsY;
        }
        std::reverse(slotsEdge4.begin(), slotsEdge4.end());

        int i = 0;
        for (Coordinate pos : slotsEdge1) {
                currX = pos.getX();
                currY = pos.getY();
                _slots.push_back({false, Coordinate(currX, currY)});
                i++;
        }

        for (Coordinate pos : slotsEdge2) {
                currX = pos.getX();
                currY = pos.getY();
                _slots.push_back({false, Coordinate(currX, currY)});
                i++;
        }

        for (Coordinate pos : slotsEdge3) {
                currX = pos.getX();
                currY = pos.getY();
                _slots.push_back({false, Coordinate(currX, currY)});
                i++;
        }

        for (Coordinate pos : slotsEdge4) {
                currX = pos.getX();
                currY = pos.getY();
                _slots.push_back({false, Coordinate(currX, currY)});
                i++;
        }
}

void IOPlacementKernel::createSections() {
        slotVector_t& slots = _slots;
        _sections.clear();
        unsigned counter = 0;
        unsigned i = 0;
        unsigned numSlots = slots.size();
        while (counter < numSlots) {
                slotVector_t nSlot;
                for (i = 0; i < _slotsPerSection && counter < numSlots; ++i) {
                        nSlot.push_back(slots[counter++]);
                }
                Section_t nSec = {nSlot, nSlot.at(nSlot.size() / 2).pos};
                if (_usagePerSection > 1.f) {
                        std::cout << "WARNING: section usage exeeded max\n";
                        _usagePerSection = 1.;
                        std::cout << "Forcing slots per section to increase\n";
                        if (_slotsIncreaseFactor != 0.0f) {
                                _slotsPerSection *= (1 + _slotsIncreaseFactor);
                        } else if (_usageIncreaseFactor != 0.0f) {
                                _slotsPerSection *= (1 + _usageIncreaseFactor);
                        } else {
                                _slotsPerSection *= 1.1;
                        }
                }
                nSec.maxSlots = _slotsPerSection * _usagePerSection;
                nSec.curSlots = 0;
                _sections.push_back(nSec);
        }
}

bool IOPlacementKernel::assignPinsSections() {
        Netlist& net = _netlistIOPins;
        sectionVector_t& sections = _sections;
        createSections();
        int totalPinsAssigned = 0;
        net.forEachIOPin([&](unsigned idx, IOPin& ioPin) {
                bool pinAssigned = false;
                std::vector<DBU> dst(sections.size());
                std::vector<InstancePin> instPinsVector;
#pragma omp parallel for
                for (unsigned i = 0; i < sections.size(); i++) {
                        dst[i] = net.computeIONetHPWL(idx, sections[i].pos);
                }
                net.forEachSinkOfIO(idx, [&](InstancePin& instPin) {
                        instPinsVector.push_back(instPin);
                });
                for (auto i : sort_indexes(dst)) {
                        if (sections[i].curSlots < sections[i].maxSlots) {
                                sections[i].net.addIONet(ioPin, instPinsVector);
                                sections[i].curSlots++;
                                pinAssigned = true;
                                totalPinsAssigned++;
                                break;
                        }
                        // Try to add pin just to first
                        if (not _forcePinSpread) break;
                }
                if (!pinAssigned) {
                        return;  // "break" forEachIOPin
                }
        });
        // if forEachIOPin ends or returns/breaks goes here
        if (totalPinsAssigned == net.numIOPins()) {
                return true;
        } else {
                return false;
        }
}

void IOPlacementKernel::setupSections() {
        if (!(_slotsPerSection > 1)) {
                std::cout << "_slotsPerSection must be grater than one\n";
                exit(1);
        }
        if (!(_usagePerSection > 0.0f)) {
                std::cout << "_usagePerSection must be grater than zero\n";
                exit(1);
        }
        if (not _forcePinSpread && _usageIncreaseFactor == 0.0f &&
            _slotsIncreaseFactor == 0.0f) {
                std::cout << "WARNING: if _forcePinSpread = false than either "
                             "_usageIncreaseFactor or _slotsIncreaseFactor "
                             "must be != 0\n";
        }
        while (not assignPinsSections()) {
                _usagePerSection *= (1 + _usageIncreaseFactor);
                _slotsPerSection *= (1 + _slotsIncreaseFactor);
                if (_sections.size() > MAX_SECTIONS_RECOMMENDED) {
                        std::cout
                            << "WARNING: number of sections is "
                            << _sections.size()
                            << " while the maximum recommended value is "
                            << MAX_SECTIONS_RECOMMENDED
                            << " this may negatively affect performance\n";
                }
                if (_slotsPerSection > MAX_SLOTS_RECOMMENDED) {
                        std::cout
                            << "WARNING: number of slots per sections is "
                            << _slotsPerSection
                            << " while the maximum recommended value is "
                            << MAX_SLOTS_RECOMMENDED
                            << " this may negatively affect performance\n";
                }
        };
}

inline void IOPlacementKernel::updateOrientation(IOPin& pin) {
        const DBU x = pin.getX();
        const DBU y = pin.getY();
        DBU lowerXBound = _core.getLowerBound().getX();
        DBU lowerYBound = _core.getLowerBound().getY();
        DBU upperXBound = _core.getUpperBound().getX();
        DBU upperYBound = _core.getUpperBound().getY();

        if (x == lowerXBound) {
                if (y == upperYBound) {
                        pin.setOrientation(Orientation::SOUTH);
                        return;
                } else {
                        pin.setOrientation(Orientation::EAST);
                        return;
                }
        }
        if (x == upperXBound) {
                if (y == lowerYBound) {
                        pin.setOrientation(Orientation::NORTH);
                        return;
                } else {
                        pin.setOrientation(Orientation::WEST);
                        return;
                }
        }
        if (y == lowerYBound) {
                pin.setOrientation(Orientation::NORTH);
                return;
        }
        if (y == upperYBound) {
                pin.setOrientation(Orientation::SOUTH);
                return;
        }
}

DBU IOPlacementKernel::returnIONetsHPWL(Netlist& netlist) {
        unsigned pinIndex = 0;
        DBU hpwl = 0;
        netlist.forEachIOPin([&](unsigned idx, IOPin& ioPin) {
                hpwl += netlist.computeIONetHPWL(idx, ioPin.getPosition());
                pinIndex++;
        });

        return hpwl / 2000;
}

void IOPlacementKernel::run() {
        std::vector<IOPin> assignment;
        std::vector<HungarianMatching> hgVec;

        initNetlistAndCore();
        initIOLists();
        defineSlots();
        setupSections();

        if (_parms->returnHPWL()) {
                std::cout << "***HPWL before IOPlacement: "
                          << returnIONetsHPWL(_netlist) << "***\n";
        }

        for (unsigned idx = 0; idx < _sections.size(); idx++) {
                if (_sections[idx].net.numIOPins() > 0) {
                        HungarianMatching hg(_sections[idx]);
                        hgVec.push_back(hg);
                }
        }

#pragma omp parallel for
        for (unsigned idx = 0; idx < hgVec.size(); idx++) {
                hgVec[idx].run();
        }

        for (unsigned idx = 0; idx < hgVec.size(); idx++) {
                hgVec[idx].getFinalAssignment(assignment, _slots);
        }

        for (auto& i : _slots) {
                if (_zeroSinkIOs.size() > 0) {
                        if (not i.used) {
                                i.used = true;
                                _zeroSinkIOs[0].setPos(i.pos);
                                assignment.push_back(_zeroSinkIOs[0]);
                                _zeroSinkIOs.erase(_zeroSinkIOs.begin());
                        }
                } else {
                        break;
                }
        }

#pragma omp parallel for
        for (unsigned i = 0; i < assignment.size(); ++i) {
                updateOrientation(assignment[i]);
        }

        WriterIOPins writer(_netlistIOPins, assignment, _horizontalMetalLayer,
                            _verticalMetalLayer, _parms->getInputDefFile(),
                            _parms->getOutputDefFile());

        writer.run();

        if (_parms->returnHPWL()) {
                DBU totalHPWL = 0;
#pragma omp parallel for reduction(+ : totalHPWL)
                for (unsigned idx = 0; idx < _sections.size(); idx++) {
                        totalHPWL += returnIONetsHPWL(_sections[idx].net);
                }
                std::cout << "***HPWL after IOPlacement: " << totalHPWL
                          << "***\n";
        }
}
