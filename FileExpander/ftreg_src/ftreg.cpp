/*
 *  ftreg (simple terminal file types registrar)
 *  Copyright (c) 2004 Ruslan Nickolaev (nruslan@hotbox.ru)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <util/application.h>
#include <storage/path.h>
#include "registrar.h"
#include <iostream>

class FRApp : public os::Application
{
    public:
        FRApp(int argc, char *argv[]);
        virtual ~FRApp();
};

FRApp::FRApp(int argc, char *argv[])
  : os::Application("application/x-vnd.nruslan-ftreg")
{
    std::cout << "Registring file type \"" << argv[1] << "\"..." << std::endl;

    try
    {
        os::RegistrarManager *pcFTManager = os::RegistrarManager::Get();
        pcFTManager->RegisterType(argv[2], argv[1], true);

        while (argc > 5)
            pcFTManager->RegisterTypeExtension(argv[2], argv[--argc]);

        pcFTManager->RegisterTypeHandler(argv[2], os::Path(argv[3]));
        pcFTManager->RegisterTypeIcon(argv[2], os::Path(argv[4]), true);
        pcFTManager->Put();
        std::cout << "Registring finished!" << std::endl;
    }

    catch (...) {
        std::cout << "Error registring file type!" << std::endl;
    }

    PostMessage(os::M_QUIT);
}

FRApp::~FRApp()
{
}

int main(int argc, char *argv[])
{
    if (argc > 4) {
        FRApp *pcApp = new FRApp(argc, argv);
        pcApp->Run();
    }

    else
        std::cout << "Simple file types registrar\nUsing: " << argv[0] << " type_name mime_type handler icon [extensions]" << std::endl;

    return 0;
}
