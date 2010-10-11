
/*
 * This file is part of pcb2gcode.
 * 
 * Copyright (C) 2009, 2010 Patrick Birnzain <pbirnzain@users.sourceforge.net>
 * 
 * pcb2gcode is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * pcb2gcode is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with pcb2gcode.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <gtkmm/main.h>

#include <glibmm/ustring.h>
using Glib::ustring;

#include "gerberimporter.hpp"
#include "surface.hpp"
#include "ngc_exporter.hpp"
#include "board.hpp"
#include "drill.hpp"
#include "options.hpp"

#include <boost/shared_ptr.hpp>
#include <boost/foreach.hpp>

#include <fstream>
#include <sstream>


int main( int argc, char* argv[] )
{
	// parse and check command line options
	Gtk::Main kit(argc, argv);
	
	options::parse_cl( argc, argv );
	po::variables_map& vm = options::get_vm();

	if( vm.count("version") ) {
		cout << PACKAGE_STRING << endl;
		exit(0);
	}

	if( vm.count("help") ) {
		cout << options::help();
		exit(0);
		
		// cout << endl << "If you're new to pcb2gcode and CNC milling, please don't forget to read the attached documentation! "
		//      << "It contains lots of valuable hints on both using this program and milling circuit boards." << endl;
	}

	options::check_parameters();

	
	// prepare environment
	shared_ptr<Isolator> isolator;
	if( vm.count("front") || vm.count("back") ) {
		isolator = shared_ptr<Isolator>( new Isolator() );
		isolator->tool_diameter = vm["offset"].as<double>() * 2;
		isolator->zwork = vm["zwork"].as<double>();
		isolator->zsafe = vm["zsafe"].as<double>();
		isolator->feed = vm["mill-feed"].as<double>();
		isolator->speed = vm["mill-speed"].as<int>();
		isolator->zchange = vm["zchange"].as<double>();
	}

	shared_ptr<Cutter> cutter;
	if( vm.count("outline") ) {
		cutter = shared_ptr<Cutter>( new Cutter() );
		cutter->tool_diameter = vm["cutter-diameter"].as<double>() - 2 * 0.005; // 2*0.005 compensates for the 10 mil outline, read doc/User_Manual.pdf
		cutter->zwork = vm["zcut"].as<double>();
		cutter->zsafe = vm["zsafe"].as<double>();
		cutter->feed = vm["cut-feed"].as<double>();
		cutter->speed = vm["cut-speed"].as<int>();
		cutter->zchange = vm["zchange"].as<double>();
		cutter->do_steps = true;
		cutter->stepsize = vm["cut-infeed"].as<double>();
	}

	shared_ptr<Driller> driller;
	if( vm.count("drill") ) {
		driller = shared_ptr<Driller>( new Driller() );
		driller->zwork = vm["zdrill"].as<double>();
		driller->zsafe = vm["zsafe"].as<double>();
		driller->feed = vm["drill-feed"].as<double>();
		driller->speed = vm["drill-speed"].as<int>();
		driller->zchange = vm["zchange"].as<double>();
	}


	shared_ptr<Board> board( new Board() );

	if( vm.count("margins") )
		board->set_margins( vm["margins"].as<double>() );

	// load files
	try
	{
		// import layer files, create surface
		cout << "Importing front side... ";
		try {
			string frontfile = vm["front"].as<string>();
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(frontfile) );
			board->prepareLayer( "front", importer, isolator, false );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( boost::exception& e ) {
			cout << "not specified\n";
		}

		cout << "Importing back side... ";
		try {
			string backfile = vm["back"].as<string>();
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(backfile) );
			board->prepareLayer( "back", importer, isolator, true );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( boost::exception& e ) {
			cout << "not specified\n";
		}

		cout << "Importing outline... ";
		try {
			string outline = vm["outline"].as<string>();
			boost::shared_ptr<LayerImporter> importer( new GerberImporter(outline) );
			board->prepareLayer( "outline", importer, cutter, !vm.count("front") );
			cout << "done\n";
		} catch( import_exception& i ) {
			cout << "error\n";
		} catch( boost::exception& e ) {
			cout << "not specified\n";
		}

	}
	catch(import_exception ie)
	{
		if( ustring const* mes = boost::get_error_info<errorstring>(ie) )
			std::cerr << "Import Error: " << *mes;
		else
			std::cerr << "Import Error: No reason given.";
	}

	try {
		board->createLayers();   // throws std::runtime_error
		cout << "Calculated board dimensions: " << board->get_width() << "in x " << board->get_height() << "in" << endl;

		shared_ptr<NGC_Exporter> exporter( new NGC_Exporter( board ) );
		exporter->add_header( PACKAGE_STRING );
		exporter->export_all();
	} catch( std::logic_error& le ) {
		cout << "Internal Error: " << le.what() << endl;
	} catch( std::runtime_error& re ) {
	}

	if( vm.count("drill") ) {
		cout << "Converting " << vm["drill"].as<string>() << "... ";

		ExcellonProcessor ep( vm["drill"].as<string>(), board->get_min_x() + board->get_max_x() );
		ep.add_header( PACKAGE_STRING );
		ep.export_ngc( vm["drill"].as<string>(), driller );

		cout << "done.\n";
	} else {
		cout << "No drill file specified.\n";
	}


}