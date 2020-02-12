/*
 *    Copyright (C)2020 by YOUR NAME HERE
 *
 *    This file is part of RoboComp
 *
 *    RoboComp is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    RoboComp is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with RoboComp.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "specificworker.h"

/**
* \brief Default constructor
*/
SpecificWorker::SpecificWorker(MapPrx& mprx) : GenericWorker(mprx)
{

	active = false;
	worldModel = AGMModel::SPtr(new AGMModel());
	worldModel->name = "worldModel";
    innerModel = std::make_shared<InnerModel>();

}

/**
* \brief Default destructor
*/
SpecificWorker::~SpecificWorker()
{
	std::cout << "Destroying SpecificWorker" << std::endl;
	emit t_compute_to_finalize();
}

bool SpecificWorker::setParams(RoboCompCommonBehavior::ParameterList params)
{
    try{ robotname = params.at("RobotName").value;}
    catch(const std::exception &e){ std::cout << e.what() << "SpecificWorker::SpecificWorker - Robot name defined in config. Using default 'robot' " << std::endl;}

    confParams  = std::make_shared<RoboCompCommonBehavior::ParameterList>(params);


	defaultMachine.start();

	return true;
}

void SpecificWorker::initialize(int period)
{
	std::cout << "Initialize worker" << std::endl;

    connect(draw_gaussian_button,SIGNAL(clicked()),&socialrules, SLOT(drawGauss()));
    connect(draw_objects_button,SIGNAL(clicked()),&socialrules, SLOT(checkObjectAffordance()));
    connect(save_data_button,SIGNAL(clicked()),&socialrules, SLOT(saveData()));
    connect(gotoperson_button,SIGNAL(clicked()),&socialrules, SLOT(goToPerson()));
    connect(follow_checkbox, SIGNAL (clicked()),&socialrules,SLOT(checkstate()));
    connect(accompany_checkbox, SIGNAL (clicked()),&socialrules,SLOT(checkstate()));
    connect(passonright_checkbox, SIGNAL (clicked()),&socialrules,SLOT(checkstate()));

    connect(object_slider, SIGNAL (valueChanged(int)),&socialrules,SLOT(affordanceSliderChanged(int)));


    socialrules.idobject_combobox = idobject_combobox;
    socialrules.idselect_combobox = idselect_combobox;
    socialrules.follow_checkbox = follow_checkbox;
    socialrules.accompany_checkbox = accompany_checkbox;
    socialrules.passonright_checkbox = passonright_checkbox;


#ifdef USE_QTGUI
	viewer = std::make_shared<InnerViewer>(innerModel, "Social Navigation");  //InnerViewer copies internally innerModel so it has to be resynchronized
#endif

    try
    {
        RoboCompAGMWorldModel::World w = agmexecutive_proxy->getModel();
        AGMExecutiveTopic_structuralChange(w);
    }
    catch(...)
    {
        printf("The executive is probably not running, waiting for first AGM model publication...");
    }

    navigation.initialize(innerModel, viewer, confParams, omnirobot_proxy);
    socialrules.initialize(worldModel, socialnavigationgaussian_proxy);

    qDebug()<<"Classes initialized correctly";

	this->Period = period;
	timer.start(Period);
    emit this->t_initialize_to_compute();


}

void SpecificWorker::compute()
{

    RoboCompLaser::TLaserData laserData = updateLaser();
	bool personMoved = false;

    if (worldModelChanged or socialrules.costChanged)
    {
		auto [changes, totalpersons, intimate_seq, personal_seq, social_seq, object_seq,
                object_lowProbVisited, object_mediumProbVisited, object_highProbVisited, objectblock_seq] = socialrules.update(worldModel);

		if (changes) //se comprueba si alguna de las personas ha cambiado de posicion
		{
			navigation.updatePolylines(totalpersons, intimate_seq, personal_seq, social_seq, object_seq, object_lowProbVisited, object_mediumProbVisited, object_highProbVisited, objectblock_seq);
			personMoved = true;
//			drawGrid();
		}

        socialrules.checkRobotmov();

        worldModelChanged = false;
    }

    navigation.update(laserData, personMoved);

    viewer->run();

}

//void SpecificWorker::drawGrid()
//{
//
//    qDebug()<<__PRETTY_FUNCTION__;
//    try	{ viewer->ts_removeNode("IMV_fmap");} catch(const QString &s){	qDebug() << s; };
//    try	{ viewer->ts_addTransform_ignoreExisting("IMV_fmap","world");} catch(const QString &s){qDebug() << s; };
//
//    try
//    {
//        uint i = 0;
//        auto grid = navigation.getMap();
//
//        for( const auto &[key,value] : navigation.getMap())
//        {
//            qDebug()<<key.x << key.z << value.cost << value.free;
//
//            QString item = "IMV_fmap_point_" + QString::number(i);
//            if(value.free)
//            {
//                if (value.cost == 1.5) //affordance spaces
//                    viewer->ts_addPlane_ignoreExisting(item, "IMV_fmap", QVec::vec3(key.x, 10, key.z), QVec::vec3(1,0,0), "#FFA200", QVec::vec3(60,60,60));
//
//                else if (value.cost == 4.0) //zona social
//                    viewer->ts_addPlane_ignoreExisting(item, "IMV_fmap", QVec::vec3(key.x, 10, key.z), QVec::vec3(1,0,0), "#00BFFF", QVec::vec3(60,60,60));
//
//                else if (value.cost == 6.0) //zona personal
//                    viewer->ts_addPlane_ignoreExisting(item, "IMV_fmap", QVec::vec3(key.x, 10, key.z), QVec::vec3(1,0,0), "#BF00FF", QVec::vec3(60,60,60));
//
//                else
//                    viewer->ts_addPlane_ignoreExisting(item, "IMV_fmap", QVec::vec3(key.x, 10, key.z), QVec::vec3(1,0,0), "#00FF00", QVec::vec3(60,60,60));
//            }
//
//            else
//                viewer->ts_addPlane_ignoreExisting(item, "IMV_fmap", QVec::vec3(key.x, 10, key.z), QVec::vec3(1,0,0), "#FF0000", QVec::vec3(60,60,60));
//
//            i++;
//         }
//
//
//    }
//
//    catch(const QString &s) {qDebug() << s;	}
//}


RoboCompLaser::TLaserData  SpecificWorker::updateLaser()
{
	RoboCompLaser::TLaserData laserData;

    try
    {
		laserData  = laser_proxy->getLaserData();
    }

    catch(const Ice::Exception &e){ std::cout <<"Can't connect to laser --" <<e.what() << std::endl; };

    return laserData;
}



void SpecificWorker::sm_compute()
{
//	std::cout<<"Entered state compute"<<std::endl;
	compute();
}

void SpecificWorker::sm_initialize()
{
	std::cout<<"Entered initial state initialize"<<std::endl;
}

void SpecificWorker::sm_finalize()
{
	std::cout<<"Entered final state finalize"<<std::endl;
}

////////////////////////// SUBSCRIPTIONS /////////////////////////////////////////////

void SpecificWorker::RCISMousePicker_setPick(const Pick &myPick)
{
    navigation.newTarget(QPointF(myPick.x,myPick.z));
}

///////////////////////////////////////////////////////////////////////////////////////

bool SpecificWorker::AGMCommonBehavior_activateAgent(const ParameterMap &prs)
{
//implementCODE
	bool activated = false;
	if (setParametersAndPossibleActivation(prs, activated))
	{
		if (not activated)
		{
			return activate(p);
		}
	}
	else
	{
		return false;
	}
	return true;
}

bool SpecificWorker::AGMCommonBehavior_deactivateAgent()
{
//implementCODE
	return deactivate();
}

ParameterMap SpecificWorker::AGMCommonBehavior_getAgentParameters()
{
//implementCODE
	return params;
}

StateStruct SpecificWorker::AGMCommonBehavior_getAgentState()
{
//implementCODE
	StateStruct s;
	if (isActive())
	{
		s.state = RoboCompAGMCommonBehavior::StateEnum::Running;
	}
	else
	{
		s.state = RoboCompAGMCommonBehavior::StateEnum::Stopped;
	}
	s.info = p.action.name;
	return s;
}

void SpecificWorker::AGMCommonBehavior_killAgent()
{
//implementCODE

}

bool SpecificWorker::AGMCommonBehavior_reloadConfigAgent()
{
//implementCODE
	return true;
}

bool SpecificWorker::AGMCommonBehavior_setAgentParameters(const ParameterMap &prs)
{
//implementCODE
	bool activated = false;
	return setParametersAndPossibleActivation(prs, activated);
}

int SpecificWorker::AGMCommonBehavior_uptimeAgent()
{
//implementCODE
	return 0;
}

void SpecificWorker::AGMExecutiveTopic_selfEdgeAdded(const int nodeid, const string &edgeType, const RoboCompAGMWorldModel::StringDictionary &attributes)
{
//subscribesToCODE
	QMutexLocker lockIM(mutex);
	try { worldModel->addEdgeByIdentifiers(nodeid, nodeid, edgeType, attributes); } catch(...){ printf("Couldn't add an edge. Duplicate?\n"); }

	try {
		innerModel = std::make_shared<InnerModel>(AGMInner::extractInnerModel(worldModel));
		navigation.innerModelChanged(innerModel);

	} catch(...) { printf("Can't extract an InnerModel from the current model.\n"); }
}

void SpecificWorker::AGMExecutiveTopic_selfEdgeDeleted(const int nodeid, const string &edgeType)
{
//subscribesToCODE
	QMutexLocker lockIM(mutex);
	try { worldModel->removeEdgeByIdentifiers(nodeid, nodeid, edgeType); } catch(...) { printf("Couldn't remove an edge\n"); }

	try {
		innerModel = std::make_shared<InnerModel>(AGMInner::extractInnerModel(worldModel));
		navigation.innerModelChanged(innerModel);

	} catch(...) { printf("Can't extract an InnerModel from the current model.\n"); }
}


void SpecificWorker::AGMExecutiveTopic_edgeUpdated(const RoboCompAGMWorldModel::Edge &modification)
{
//subscribesToCODE


	QMutexLocker locker(mutex);

	AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);
	AGMInner::updateImNodeFromEdge(worldModel, modification, innerModel.get());

    auto symbol1 = worldModel->getSymbolByIdentifier(modification.a);
    auto symbol2 = worldModel->getSymbolByIdentifier(modification.b);

    if(symbol1.get()->symbolType == "person" or symbol2.get()->symbolType == "person")
        worldModelChanged = true;


}

void SpecificWorker::AGMExecutiveTopic_edgesUpdated(const RoboCompAGMWorldModel::EdgeSequence &modifications)
{
//subscribesToCODE


	QMutexLocker lockIM(mutex);
	for (auto modification : modifications)
	{
	    auto symbol1 = worldModel->getSymbolByIdentifier(modification.a);
        auto symbol2 = worldModel->getSymbolByIdentifier(modification.b);

        if(symbol1.get()->symbolType == "person" or symbol2.get()->symbolType == "person")
            worldModelChanged = true;

		AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);
		AGMInner::updateImNodeFromEdge(worldModel, modification, innerModel.get());
	}


}

void SpecificWorker::AGMExecutiveTopic_structuralChange(const RoboCompAGMWorldModel::World &w)
{

	QMutexLocker lockIM(mutex);
	static bool first = true;
    qDebug() << "structural Change";

	AGMModelConverter::fromIceToInternal(w, worldModel);
	innerModel.reset(AGMInner::extractInnerModel(worldModel));
//	innerModel = std::make_shared<InnerModel>(AGMInner::extractInnerModel(worldModel));

	if (!first)
	{
		innerModel->save("/home/robocomp/robocomp/components/robocomp-viriato/etcSim/innermodel.xml");
	}
	else
		first = false;

	navigation.innerModelChanged(innerModel);
	viewer->reloadInnerModel(innerModel);
    worldModelChanged = true;

}

void SpecificWorker::AGMExecutiveTopic_symbolUpdated(const RoboCompAGMWorldModel::Node &modification)
{
//subscribesToCODE

	QMutexLocker locker(mutex);
	AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);


}

void SpecificWorker::AGMExecutiveTopic_symbolsUpdated(const RoboCompAGMWorldModel::NodeSequence &modifications)
{
//subscribesToCODE
	QMutexLocker l(mutex);


    for (auto modification : modifications)
		AGMModelConverter::includeIceModificationInInternalModel(modification, worldModel);

}



bool SpecificWorker::setParametersAndPossibleActivation(const ParameterMap &prs, bool &reactivated)
{
	printf("<<< setParametersAndPossibleActivation\n");
	// We didn't reactivate the component
	reactivated = false;

	// Update parameters
	params.clear();
	for (ParameterMap::const_iterator it=prs.begin(); it!=prs.end(); it++)
	{
		params[it->first] = it->second;
	}

	try
	{
		action = params["action"].value;
		std::transform(action.begin(), action.end(), action.begin(), ::tolower);
		//TYPE YOUR ACTION NAME
		if (action == "actionname")
		{
			active = true;
		}
		else
		{
			active = true;
		}
	}
	catch (...)
	{
		printf("exception in setParametersAndPossibleActivation %d\n", __LINE__);
		return false;
	}

	// Check if we should reactivate the component
	if (active)
	{
		active = true;
		reactivated = true;
	}

	printf("setParametersAndPossibleActivation >>>\n");

	return true;
}
void SpecificWorker::sendModificationProposal(AGMModel::SPtr &worldModel, AGMModel::SPtr &newModel)
{
	try
	{
		AGMMisc::publishModification(newModel, agmexecutive_proxy, "socialNavigationAgentAgent");
	}
/*	catch(const RoboCompAGMExecutive::Locked &e)
	{
	}
	catch(const RoboCompAGMExecutive::OldModel &e)
	{
	}
	catch(const RoboCompAGMExecutive::InvalidChange &e)
	{
	}
*/
	catch(const Ice::Exception& e)
	{
		exit(1);
	}
}
