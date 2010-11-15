/*
Copyright (C) 2007 <SWGEmu>

This File is part of Core3.

This program is free software; you can redistribute
it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software
Foundation; either version 2 of the License,
or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General
Public License along with this program; if not, write to
the Free Software Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Linking Engine3 statically or dynamically with other modules
is making a combined work based on Engine3.
Thus, the terms and conditions of the GNU Lesser General Public License
cover the whole combination.

In addition, as a special exception, the copyright holders of Engine3
give you permission to combine Engine3 program with free software
programs or libraries that are released under the GNU LGPL and with
code included in the standard release of Core3 under the GNU LGPL
license (or modified versions of such code, with unchanged license).
You may copy and distribute such a system following the terms of the
GNU LGPL for Engine3 and the licenses of the other code concerned,
provided that you include the source code of that other code when
and as the GNU LGPL requires distribution of source code.

Note that people who make modified versions of Engine3 are not obligated
to grant this special exception for their modified versions;
it is their choice whether to do so. The GNU Lesser General Public License
gives permission to release a modified version without this exception;
this exception also makes it possible to release a modified version
which carries forward this exception.
*/

#include "LoginServer.h"
#include "LoginClient.h"

#include "LoginProcessServerImplementation.h"

#include "LoginMessageProcessorTask.h"

#include "server/conf/ConfigManager.h"
#include "server/login/account/AccountManager.h"
#include "server/login/account/Account.h"

#include "LoginHandler.h"

#include "objects.h"

LoginServerImplementation::LoginServerImplementation(ConfigManager* configMan) :
		ManagedServiceImplementation(), Logger("LoginServer") {

	phandler = NULL;

	datagramService = new DatagramServiceThread("LoginServer");
	datagramService->setLogging(false);
	datagramService->setLockName("LoginServerLock");

	loginHandler = new LoginHandler();
	datagramService->setHandler(loginHandler);

	configManager = configMan;

	processor = NULL;

	enumClusterMessage = NULL;
	clusterStatusMessage = NULL;

	accountManager = NULL;

	setLogging(true);
}

void LoginServerImplementation::initializeTransientMembers() {
	phandler = NULL;

	processor = NULL;

	ManagedObjectImplementation::initializeTransientMembers();
}

void LoginServerImplementation::initialize() {
	processor = new LoginProcessServerImplementation(_this);
	processor->initialize();

	phandler = new BasePacketHandler("LoginServer", loginHandler);
	phandler->setLogging(false);

	startManagers();

	//taskManager->setLogging(false);

	populateGalaxyList();

	return;
}


void LoginServerImplementation::startManagers() {
	info("loading managers..");

	accountManager = new AccountManager(_this);
	accountManager->setAutoRegistrationEnabled(configManager->getAutoReg());
	accountManager->setRequiredVersion(configManager->getLoginRequiredVersion());
	accountManager->setMaxOnlineCharacters(configManager->getZoneOnlineCharactersPerAccount());
}

void LoginServerImplementation::start(int p, int mconn) {
	loginHandler->setLoginSerrver(_this);

	datagramService->start(p, mconn);
}

void LoginServerImplementation::stop() {
	datagramService->stop();
}

void LoginServerImplementation::shutdown() {
	stopManagers();

	printInfo();

	info("shut down complete", true);
}

void LoginServerImplementation::stopManagers() {
	accountManager = NULL;

	info("managers stopped", true);
}

ServiceClient* LoginServerImplementation::createConnection(Socket* sock, SocketAddress& addr) {
	LoginClient* client = new LoginClient(datagramService, sock, addr);

	info("client connected from \'" + client->getAddress() + "\'");

	return client;
}

void LoginServerImplementation::handleMessage(ServiceClient* client, Packet* message) {
	LoginClient* lclient = (LoginClient*) client;

	try {
		if (lclient->isAvailable())
			phandler->handlePacket(lclient, message);

	} catch (PacketIndexOutOfBoundsException& e) {
		System::out << e.getMessage();

		error("incorrect packet - " + message->toStringData());
	} catch (DatabaseException& e) {
		error(e.getMessage());
	} catch (ArrayIndexOutOfBoundsException& e) {
		error(e.getMessage());
	} catch (...) {
		System::out << "[LoginServer] unreported Exception caught\n";
	}
}

void LoginServerImplementation::processMessage(Message* message) {
	//info("processing message " + message->toStringData());

	Task* task = new LoginMessageProcessorTask(message, processor->getPacketHandler());

	Core::getTaskManager()->executeTask(task);
}

bool LoginServerImplementation::handleError(ServiceClient* client, Exception& e) {
	LoginClient* lclient = (LoginClient*) client;
	lclient->setError();

	lclient->disconnect();

	return true;
}

void LoginServerImplementation::printInfo() {
	lock();

	StringBuffer msg;
	msg << "MessageQueue - size = " << datagramService->getMessageQueue()->size();
	info(msg, true);

	unlock();
}

void LoginServerImplementation::populateGalaxyList() {
	//Populate the galaxies list for the login server.
	GalaxyList galaxies;
	uint32 galaxyCount = galaxies.size();

	//In case we want to add the functionality to update the lists while the server is running...
	if (enumClusterMessage != NULL) {
		delete enumClusterMessage;
		enumClusterMessage = NULL;
	}

	if (clusterStatusMessage != NULL) {
		delete enumClusterMessage;
		enumClusterMessage = NULL;
	}

	enumClusterMessage = new LoginEnumCluster(galaxyCount);
	clusterStatusMessage = new LoginClusterStatus(galaxyCount);

    while (galaxies.next()) {
    	uint32 galaxyID = galaxies.getGalaxyID();

    	String name;
    	galaxies.getGalaxyName(name);

    	enumClusterMessage->addGalaxy(galaxyID, name);

		String address;
    	galaxies.getGalaxyAddress(address);

    	clusterStatusMessage->addGalaxy(galaxyID, address, galaxies.getGalaxyPort(), galaxies.getGalaxyPingPort());
    }
}
