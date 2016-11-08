//This file will compute the spatial facts concerning agents present in the interaction.

#include "toaster_msgs/ToasterHumanReader.h"
#include "toaster_msgs/ToasterRobotReader.h"
#include "toaster_msgs/ToasterObjectReader.h"
#include "toaster_msgs/AddArea.h"
#include "toaster_msgs/RemoveArea.h"
#include "toaster_msgs/PrintArea.h"
#include "toaster_msgs/Empty.h"
#include "toaster_msgs/GetRelativePosition.h"
#include "toaster_msgs/GetMultiRelativePosition.h"
#include "toaster_msgs/Area.h"
#include "toaster_msgs/AreaList.h"
#include "toaster-lib/CircleArea.h"
#include "toaster-lib/PolygonArea.h"
#include "toaster-lib/MathFunctions.h"
#include "toaster-lib/Object.h"
#include <toaster_msgs/Fact.h>
#include <toaster_msgs/FactList.h>
#include <geometry_msgs/PolygonStamped.h>
#include <iterator>
//#include <boost/numeric/ublas/matrix.hpp>
//#include <boost/numeric/ublas/io.hpp>


//namespace trans = bg::strategy::transform;
typedef bg::model::point<double, 2, bg::cs::cartesian> point_type;
//namespace bn = boost::numeric;

// Vector of Area
// It should be possible to add an area on the fly with a ros service.
std::map<unsigned int, Area*> mapArea_;
std::map<std::string, Entity*> mapEntities_;

// Publisher for area
bool publishingArea_ = true;

ros::NodeHandle* node_;

geometry_msgs::Polygon polygonToRos(unsigned int id) {
    geometry_msgs::Polygon poly;
    geometry_msgs::Point32 curPoint;
    std::vector<bg::model::d2::point_xy<double> > polyPoints = ((PolygonArea*) mapArea_[id])->poly_.outer();
    for (unsigned int i = 0; i < polyPoints.size(); ++i) {
        curPoint.x = polyPoints[i].get<0>();
        curPoint.y = polyPoints[i].get<1>();
        poly.points.push_back(curPoint);
    }
    return poly;
}

void setAreaMsg(toaster_msgs::AreaList& areaList_msg) {
    toaster_msgs::Area area;
    for (std::map<unsigned int, Area*>::iterator it = mapArea_.begin(); it != mapArea_.end(); ++it) {
        area.id = it->second->getId();
        area.name = it->second->getName();

        //If it is a circle area
        if (it->second->getIsCircle()) {
            area.center.x = ((CircleArea*) it->second)->getCenter().get<0>();
            area.center.y = ((CircleArea*) it->second)->getCenter().get<1>();
            area.ray = ((CircleArea*) it->second)->getRay();

        } else {
            //If it is a polygon
            area.poly = polygonToRos(it->first);
        }

        area.isCircle = it->second->getIsCircle();
        area.entityType = it->second->getEntityType();
        area.factType = it->second->getFactType();
        area.myOwner = it->second->getMyOwner();
        area.areaType = it->second->getAreaType();

        areaList_msg.areaList.push_back(area);
    }

}

bool areaCompatible(std::string areaEntType, EntityType entType) {
    if (entType == ROBOT) {
        if (areaEntType == "robots" || areaEntType == "agents" || areaEntType == "entities")
            return true;
        else
            return false;

    } else if (entType == HUMAN) {
        if (areaEntType == "humans" || areaEntType == "agents" || areaEntType == "entities")
            return true;
        else
            return false;

    } else if (entType == OBJECT) {
        if (areaEntType == "objects" || areaEntType == "entities")
            return true;
        else
            return false;
    }
}

// Entity should be a vector or a map with all entities
// This function update all the area that depends on an entity position.

// Boost > 1.55

/*void updateEntityArea(std::map<unsigned int, Area*>& mpArea, Entity * entity) {
    for (std::map<unsigned int, Area*>::iterator it = mpArea.begin(); it != mpArea.end(); ++it) {
        if (it->second->getMyOwner() == entity->getId()) {

            // Owner 2D frame
            trans::translate_transformer<double, 2, 2> translate(entity->getPosition().get<0>(), entity->getPosition().get<1>());
            trans::rotate_transformer<bg::radian, double, 2, 2> rotate(-entity->getOrientation()[2]);

            // Rotate, then translate
            trans::ublas_transformer<double, 2, 2> rotateTranslate(bn::ublas::prod(translate.matrix(), rotate.matrix()));

            if (it->second->getIsCircle()) {

                point_type newCenter;
                rotateTranslate.apply(((CircleArea*) it->second)->getCenterRelative(), newCenter);
                ((CircleArea*) it->second)->setCenter(newCenter);

            } else {

                bg::model::polygon<bg::model::d2::point_xy<double> > newPoly;
                //rotateTranslate.apply(((PolygonArea*) it->second)->getPolyRelative(), newPoly);
                bg::transform(((PolygonArea*) it->second)->getPolyRelative(), newPoly, rotateTranslate);
                ((PolygonArea*) it->second)->poly_ = newPoly;


            }
        }
    }
}*/

void rotateTranslate(Entity* rotEnt, CircleArea* circleArea) {

    double theta = rotEnt->getOrientation()[2];
    point_type newCenter;

    newCenter.set<0>(cos(theta) * circleArea->getCenterRelative().get<0>() - sin(theta) * circleArea->getCenterRelative().get<1>());
    newCenter.set<1>(sin(theta) * circleArea->getCenterRelative().get<0>() + cos(theta) * circleArea->getCenterRelative().get<1>());

    newCenter.set<0>(newCenter.get<0>() + rotEnt->getPosition().get<0>());
    newCenter.set<1>(newCenter.get<1>() + rotEnt->getPosition().get<1>());
    circleArea->setCenter(newCenter);
}

void rotateTranslate(Entity* rotEnt, PolygonArea* polyArea) {

    double theta = rotEnt->getOrientation()[2];
    bg::model::polygon<bg::model::d2::point_xy<double> > newPoly;

    std::vector<bg::model::d2::point_xy<double> > polyPointsRelative = polyArea->getPolyRelative().outer();

    //Just to init with something...
    std::vector<bg::model::d2::point_xy<double> > polyPoints = polyArea->getPolyRelative().outer();


    for (unsigned int i = 0; i < polyPoints.size(); ++i) {

        polyPoints[i].set<0>(cos(theta) * polyPointsRelative[i].get<0>() - sin(theta) * polyPointsRelative[i].get<1>());
        polyPoints[i].set<1>(sin(theta) * polyPointsRelative[i].get<0>() + cos(theta) * polyPointsRelative[i].get<1>());

        polyPoints[i].set<0>(polyPoints[i].get<0>() + rotEnt->getPosition().get<0>());
        polyPoints[i].set<1>(polyPoints[i].get<1>() + rotEnt->getPosition().get<1>());
        bg::append(newPoly, polyPoints[i]);

    }
    polyArea->poly_ = newPoly;
}

void updateEntityArea(std::map<unsigned int, Area*>& mpArea, Entity * entity) {
    for (std::map<unsigned int, Area*>::iterator it = mpArea.begin(); it != mpArea.end(); ++it) {
        if (it->second->getMyOwner() == entity->getId()) {

            if (it->second->getIsCircle()) {
                rotateTranslate(entity, ((CircleArea*) it->second));
            } else {
                rotateTranslate(entity, ((PolygonArea*) it->second));
            }
        }
    }
}

void updateInArea(Entity* ent, std::map<unsigned int, Area*>& mpArea) {
    for (std::map<unsigned int, Area*>::iterator it = mpArea.begin(); it != mpArea.end(); ++it) {
        // if the entity is actually concerned, and is not the owner
        if (areaCompatible(it->second->getEntityType(), ent->getEntityType()) && it->second->getMyOwner() != ent->getId()) {

            // If we already know that entity is in Area, we update if needed.
            if (ent->isInArea(it->second->getId()))
                if (it->second->isPointInArea(MathFunctions::convert3dTo2d(ent->getPosition())))
                    continue;
                else {
                    printf("[area_manager] %s leaves Area %s\n", ent->getName().c_str(), it->second->getName().c_str());
                    ent->removeInArea(it->second->getId());
                    it->second->removeEntity(ent->getId());
                    if (it->second->getAreaType() == "room")
                        ent->setRoomId(0);
                }// Same if entity is not in Area
            else
                if (it->second->isPointInArea(MathFunctions::convert3dTo2d(ent->getPosition()))) {
                printf("[area_manager] %s enters in Area %s\n", ent->getName().c_str(), it->second->getName().c_str());
                ent->inArea_.push_back(it->second->getId());
                it->second->insideEntities_.push_back(ent->getId());

                //User has to be in a room. May it be a "global room".
                if (it->second->getAreaType() == "room")
                    ent->setRoomId(it->second->getId());
            } else {
                //printf("[area_manager][DEGUG] %s is not in Area %s, he is in %f, %f\n", ent->getName().c_str(), it->second->getName().c_str(), ent->getPosition().get<0>(), ent->getPosition().get<1>());
                continue;
            }
        } else {
            continue;
        }
    }
}

// Return confidence: 0.0 if not facing 1.0 if facing

double isFacing(Entity* entFacing, Entity* entSubject, double angleThreshold, double& angleResult) {
    return MathFunctions::isInAngle(entFacing, entSubject, entFacing->getOrientation()[2], angleThreshold, angleResult);
}

void printMyArea(unsigned int id) {
    if (mapArea_[id]->getIsCircle())
        printf("Area name: %s, id: %d, owner id: %s, type %s, factType: %s \n"
            "entityType: %s, isCircle: true, center: %f, %f, ray: %f\n", mapArea_[id]->getName().c_str(),
            id, mapArea_[id]->getMyOwner().c_str(), mapArea_[id]->getAreaType().c_str(), mapArea_[id]->getFactType().c_str(), mapArea_[id]->getEntityType().c_str(),
            ((CircleArea*) mapArea_[id])->getCenter().get<0>(), ((CircleArea*) mapArea_[id])->getCenter().get<1>(), ((CircleArea*) mapArea_[id])->getRay());
    else
        printf("Area name: %s, id: %d, owner id: %s, type %s, factType: %s \n"
            "entityType: %s, isCircle: false\n", mapArea_[id]->getName().c_str(),
            id, mapArea_[id]->getMyOwner().c_str(), mapArea_[id]->getAreaType().c_str(), mapArea_[id]->getFactType().c_str(), mapArea_[id]->getEntityType().c_str());

    printf("inside entities: ");
    for (int i = 0; i < mapArea_[id]->insideEntities_.size(); i++)
        printf(" %s,", mapArea_[id]->insideEntities_[i].c_str());

    printf("\n--------------------------\n");
}

unsigned int getFreeId(std::map<unsigned int, Area*>& map) {
    unsigned int i = 1;
    for (std::map<unsigned int, Area*>::iterator it = map.begin(); it != map.end(); ++it)
        if (i == it->first)
            i++;
        else
            break;
    return i;
}


///////////////////////////
//   Service functions   //
///////////////////////////

bool addArea(toaster_msgs::AddArea::Request &req,
        toaster_msgs::AddArea::Response & res) {

    Area* curArea;

    unsigned int id;
    // If no id, get one
    if (req.myArea.id == 0)
        id = getFreeId(mapArea_);
    else
        id = req.myArea.id;


    //If it is a circle area
    if (req.myArea.isCircle) {
        bg::model::point<double, 2, bg::cs::cartesian> center(req.myArea.center.x, req.myArea.center.y);

        CircleArea* myCircle = new CircleArea(id, center, req.myArea.ray);
        curArea = myCircle;
    } else {
        //If it is a polygon
        double pointsPoly[req.myArea.poly.points.size()][2];
        for (int i = 0; i < req.myArea.poly.points.size(); i++) {
            pointsPoly[i][0] = req.myArea.poly.points[i].x;
            pointsPoly[i][1] = req.myArea.poly.points[i].y;
        }

        PolygonArea* myPoly = new PolygonArea(id, pointsPoly, req.myArea.poly.points.size());
        curArea = myPoly;
    }

    curArea->setIsCircle(req.myArea.isCircle);
    curArea->setEntityType(req.myArea.entityType);
    curArea->setFactType(req.myArea.factType);
    curArea->setMyOwner(req.myArea.myOwner);
    curArea->setName(req.myArea.name);
    curArea->setAreaType(req.myArea.areaType);

    mapArea_[curArea->getId()] = curArea;

    res.answer = true;
    ROS_INFO("request: added Area: id %d, name %s", req.myArea.id, req.myArea.name.c_str());
    ROS_INFO("sending back response: [%d]", (int) res.answer);
    return true;
}

bool removeArea(toaster_msgs::RemoveArea::Request &req,
        toaster_msgs::RemoveArea::Response & res) {

    ROS_INFO("request: removed Area: id %d, named: %s", req.id, mapArea_[req.id]->getName().c_str());
    if (mapArea_.find(req.id) != mapArea_.end())
        mapArea_.erase(req.id);

    res.answer = true;
    ROS_INFO("sending back response: [%d]", (int) res.answer);
    return true;
}

bool removeAllAreas(toaster_msgs::Empty::Request &req,
        toaster_msgs::Empty::Response & res) {

    ROS_INFO("request: remove all areas");
    mapArea_.clear();
    return true;
}

bool printArea(toaster_msgs::PrintArea::Request &req,
        toaster_msgs::RemoveArea::Response & res) {

    std::map<unsigned int, Area*>::iterator itArea = mapArea_.find(req.id);
    if (itArea != mapArea_.end()) {
        ROS_WARN("The requested area with id %d does not exist, please enter a valid id.\n "
                "Alternatively, use the service /area_manager/print_all_areas", req.id);
        res.answer = false;
        return false;
    }
    printMyArea(req.id);
    res.answer = true;
    return true;
}

bool printAllAreas(toaster_msgs::Empty::Request &req,
        toaster_msgs::Empty::Response & res) {
    for (std::map<unsigned int, Area*>::iterator itArea = mapArea_.begin(); itArea != mapArea_.end(); ++itArea)
        printMyArea(itArea->first);
    return true;
}

// This function is used to get relative position of an entity according to another (left / right))

bool getRelativePosition(toaster_msgs::GetRelativePosition::Request &req,
        toaster_msgs::GetRelativePosition::Response & res) {
    double pi = 3.1416;

    if (mapEntities_.find(req.subjectId) != mapEntities_.end() && mapEntities_.find(req.targetId) != mapEntities_.end()) {
        double angleResult;
        angleResult = MathFunctions::relativeAngle(mapEntities_[req.subjectId], mapEntities_[req.targetId], mapEntities_[req.subjectId]->getOrientation()[2]);
        if (angleResult > 0) {
            if (angleResult < pi / 6)
                res.direction = "ahead";
            else if (angleResult < pi / 4)
                res.direction = "ahead right";
            else if (angleResult < 3 * pi / 4)
                res.direction = "right";
            else if (angleResult < 5 * pi / 6)
                res.direction = "back right";
            else
                res.direction = "back";
        } else {
            if (-angleResult < pi / 6)
                res.direction = "ahead";
            else if (-angleResult < pi / 4)
                res.direction = "ahead left";
            else if (-angleResult < 3 * pi / 4)
                res.direction = "left";
            else if (-angleResult < 5 * pi / 6)
                res.direction = "back left";
            else
                res.direction = "back";
        }

        res.answer = true;
        res.angleValue = angleResult;
        return true;
    } else
        ROS_INFO("Requested entity is not in the list.");
    res.answer = false;
    return false;
}

// This function is used to get relative position of an entity according to another (left / right)) in an agent point of view

bool getMultiRelativePosition(toaster_msgs::GetMultiRelativePosition::Request &req,
        toaster_msgs::GetMultiRelativePosition::Response & res) {
    double pi = 3.1416;

    // [TODO] Replace with find
    if (mapEntities_[req.agentSubjectId] != NULL && mapEntities_[req.objectSubjectId] != NULL && mapEntities_[req.targetId] != NULL) {
        double angleSubjects, angleResult;
        angleSubjects = MathFunctions::relativeAngle(mapEntities_[req.agentSubjectId], mapEntities_[req.objectSubjectId], 0);
        angleResult = MathFunctions::relativeAngle(mapEntities_[req.objectSubjectId], mapEntities_[req.targetId], angleSubjects);
        if (angleResult > 0) {
            if (angleResult < pi / 6)
                res.direction = "ahead";
            else if (angleResult < pi / 4)
                res.direction = "ahead right";
            else if (angleResult < 3 * pi / 4)
                res.direction = "right";
            else if (angleResult < 5 * pi / 6)
                res.direction = "back right";
            else
                res.direction = "back";
        } else {
            if (-angleResult < pi / 6)
                res.direction = "ahead";
            else if (-angleResult < pi / 4)
                res.direction = "ahead left";
            else if (-angleResult < 3 * pi / 4)
                res.direction = "left";
            else if (-angleResult < 5 * pi / 6)
                res.direction = "back left";
            else
                res.direction = "back";
        }

        res.answer = true;
        res.angleValue = angleResult;
        return true;
    } else
        ROS_INFO("Requested entity is not in the list.");
    res.answer = false;
    return false;
}

bool publishAllAreas(toaster_msgs::Empty::Request &req,
        toaster_msgs::Empty::Response & res) {

    publishingArea_ = !publishingArea_;
    ROS_INFO("request: publishing area: %d", publishingArea_);
    return true;
}

int main(int argc, char** argv) {
    // Set this in a ros service
    const bool AGENT_FULL_CONFIG = true; //If false we will use only position and orientation

    ros::init(argc, argv, "area_manager");
    ros::NodeHandle node;
    node_ = &node;

    //Data reading
    ToasterHumanReader humanRd(node, AGENT_FULL_CONFIG);
    ToasterRobotReader robotRd(node, AGENT_FULL_CONFIG);
    ToasterObjectReader objectRd(node);

    //Services
    ros::ServiceServer serviceAdd = node.advertiseService("area_manager/add_area", addArea);
    ROS_INFO("Ready to add Area.");

    ros::ServiceServer serviceRemove = node.advertiseService("area_manager/remove_area", removeArea);
    ROS_INFO("Ready to remove Area.");

    ros::ServiceServer serviceRemoveAll = node.advertiseService("area_manager/remove_all_areas", removeAllAreas);
    ROS_INFO("Ready to remove Area.");

    ros::ServiceServer servicePrint = node.advertiseService("area_manager/print_area", printArea);
    ROS_INFO("Ready to print Area.");

    ros::ServiceServer servicePrintAll = node.advertiseService("area_manager/print_all_areas", printAllAreas);
    ROS_INFO("Ready to print Areas.");

    ros::ServiceServer serviceRelativePose = node.advertiseService("area_manager/get_relative_position", getRelativePosition);
    ROS_INFO("Ready to print get relative position.");

    ros::ServiceServer serviceMultiRelativePose = node.advertiseService("area_manager/get_multiple_relative_position", getMultiRelativePosition);
    ROS_INFO("Ready to print get relative position in an agent perspective.");

    ros::ServiceServer servicepublishAllArea = node.advertiseService("area_manager/publish_all_areas", publishAllAreas);
    ROS_INFO("Ready to publish all areas.");

    // Publishing
    ros::Publisher fact_pub = node.advertise<toaster_msgs::FactList>("area_manager/factList", 1000);
    ros::Publisher area_pub = node_->advertise<toaster_msgs::AreaList>("area_manager/areaList", 1000);

    // Set this in a ros service?
    ros::Rate loop_rate(30);



    /************************/
    /* Start of the Ros loop*/
    /************************/

    //TODO: remove human / robot id and do it for all
    while (node.ok()) {
        toaster_msgs::FactList factList_msg;
        toaster_msgs::Fact fact_msg;

        toaster_msgs::AreaList areaList_msg;

        ////////////////////////////////
        // Updating situational Areas //
        ////////////////////////////////

        //TODO: for more optimal computing, go through area and if it is a circle, update area.
        // for each area
        //   if current area is circle
        //     get area owner
        //     update area with owner position

        // Humans
        for (std::map<std::string, Human*>::iterator it = humanRd.lastConfig_.begin(); it != humanRd.lastConfig_.end(); ++it) {
            // We update area with human center
            for(std::map<std::string, Joint*>::iterator it2 = it->second->skeleton_.begin() ; it2 != it->second->skeleton_.end() ; ++it2)
			    mapEntities_[it2->first]=it2->second;
            mapEntities_[it->first] = it->second;
            updateEntityArea(mapArea_, it->second);
        }

        // Robots
        for (std::map<std::string, Robot*>::iterator it = robotRd.lastConfig_.begin(); it != robotRd.lastConfig_.end(); ++it) {
            // We update area with robot center
            mapEntities_[it->first] = it->second;
            updateEntityArea(mapArea_, it->second);
        }

        // Objects
        for (std::map<std::string, Object*>::const_iterator it = objectRd.lastConfig_.begin(); it != objectRd.lastConfig_.end(); ++it) {
            // We update area with object center
            mapEntities_[it->first] = it->second;
            updateEntityArea(mapArea_, it->second);
        }


        /////////////////////////////////
        // Updating in Area properties //
        /////////////////////////////////


        for (std::map<std::string, Entity*>::iterator it = mapEntities_.begin(); it != mapEntities_.end(); ++it) {
            // We update area with owners
            updateInArea(it->second, mapArea_);
        }

        ///////////////////////////////////////
        // Computing facts for each entities //
        ///////////////////////////////////////

        // TODO: replace code by a function for each fact computation

        for (std::map<unsigned int, Area*>::iterator itArea = mapArea_.begin(); itArea != mapArea_.end(); ++itArea) {
            double areaDensity = 0.0;
            unsigned long densityTime = 0;

            Entity* ownerEnt = NULL;

            // Computation depending on owner
            if (itArea->second->getMyOwner() != "") {

                // Let's find back the area owner:
                if (robotRd.lastConfig_.find(itArea->second->getMyOwner()) != robotRd.lastConfig_.end())
                    ownerEnt = robotRd.lastConfig_[itArea->second->getMyOwner()];

                else if (humanRd.lastConfig_.find(itArea->second->getMyOwner()) != humanRd.lastConfig_.end())
                    ownerEnt = humanRd.lastConfig_[itArea->second->getMyOwner()];

                else if (objectRd.lastConfig_.find(itArea->second->getMyOwner()) != objectRd.lastConfig_.end())
                    ownerEnt = objectRd.lastConfig_[itArea->second->getMyOwner()];
            }

            

            for (std::map<std::string, Entity*>::iterator itEntity = mapEntities_.begin(); itEntity != mapEntities_.end(); ++itEntity) {

                if (itEntity->second->isInArea(itArea->first)) {


                    // compute facts according to factType
                    // TODO: instead of calling it interaction, make a list of facts to compute?




                    if (itArea->second->getFactType() == "interaction") {

                        // If it is an interacting area, we need the owner!
                        if (ownerEnt != NULL) {

                            // Now let's compute isFacing
                            //////////////////////////////

                            double confidence = 0.0;
                            // This is the actual angle between subject orientation
                            // and target. It gives left / right relation
                            // If positive, target is at right!
                            double angleResult = 0.0;
                            confidence = isFacing(itEntity->second, ownerEnt, 0.5, angleResult);
                            if (confidence > 0.0) {
                                printf("[area_manager][DEBUG] %s is facing %s with confidence %f, angleResult %f\n",
                                        itEntity->second->getName().c_str(), ownerEnt->getName().c_str(), confidence, angleResult);

                                //Fact Facing
                                fact_msg.property = "IsFacing";
                                fact_msg.propertyType = "posture";
                                fact_msg.subProperty = "angle";
                                fact_msg.subjectId = itEntity->first;
                                fact_msg.targetId = ownerEnt->getId();
                                fact_msg.confidence = confidence;
                                fact_msg.stringValue = true;
                                fact_msg.doubleValue = angleResult;
                                fact_msg.valueType = 0;
                                fact_msg.factObservability = 0.5;
                                fact_msg.time = itEntity->second->getTime();

                                factList_msg.factList.push_back(fact_msg);
                            }


                            // Compute here other facts linked to interaction
                            //////////////////////////////////////////////////

                            // TODO


                        } // ownerEnt!= NULL

                    } else if (itArea->second->getFactType() == "density") {
                        areaDensity += 1.0;
                        densityTime = itEntity->second->getTime();
                    } else if (itArea->second->getFactType() == "") {

                    } else {
                        printf("[area_manager][WARNING] Area %s has factType %s, which is not available\n", itArea->second->getName().c_str(), itArea->second->getFactType().c_str());
                    }

                    if (itArea->second->getAreaType() == "room") {
                        //Fact in Area
                        fact_msg.property = "IsInRoom";
                        fact_msg.propertyType = "position";

                        fact_msg.subProperty = itArea->second->getAreaType();
                        if (ownerEnt != NULL) {
                            fact_msg.targetOwnerId = ownerEnt->getId();
                        }

                        fact_msg.subjectId = itEntity->first;
                        fact_msg.targetId = itArea->second->getName();
                        fact_msg.confidence = 1;
                        fact_msg.factObservability = 0.8;
                        fact_msg.time = itEntity->second->getTime();
                        fact_msg.valueType = 0;
                        fact_msg.stringValue = "true";

                        factList_msg.factList.push_back(fact_msg);
                    } else if (itArea->second->getAreaType() == "support") {
                        //Fact in Area
                        fact_msg.property = "IsAt";
                        fact_msg.propertyType = "position";

                        fact_msg.subProperty = "location";
                        if (ownerEnt != NULL) {
                            fact_msg.targetOwnerId = ownerEnt->getId();
                        }

                        fact_msg.subjectId = itEntity->first;
                        fact_msg.targetId = itArea->second->getName();
                        fact_msg.confidence = 1;
                        fact_msg.factObservability = 0.8;
                        fact_msg.time = itEntity->second->getTime();
                        fact_msg.valueType = 0;
                        fact_msg.stringValue = "true";

                        factList_msg.factList.push_back(fact_msg);

                    } else {
                        //Fact in Area
                        fact_msg.property = "IsInArea";
                        fact_msg.propertyType = "position";

                        fact_msg.subProperty = itArea->second->getAreaType();
                        if (ownerEnt != NULL) {
                            fact_msg.targetOwnerId = ownerEnt->getId();
                        }

                        fact_msg.subjectId = itEntity->first;
                        fact_msg.targetId = itArea->second->getName();
                        fact_msg.confidence = 1;
                        fact_msg.factObservability = 0.8;
                        fact_msg.time = itEntity->second->getTime();

                        factList_msg.factList.push_back(fact_msg);
                    }
                }
            }// For all Entities

            // We compute here the density
            if (itArea->second->getFactType() == "density") {
                // -1 is a hack to remove centroid
                int fullPopulation = -1;
                if (itArea->second->getEntityType() == "humans" || itArea->second->getEntityType() == "agents"
                        || itArea->second->getEntityType() == "entities")
                    fullPopulation += humanRd.lastConfig_.size();

                if (itArea->second->getEntityType() == "robots" || itArea->second->getEntityType() == "agents"
                        || itArea->second->getEntityType() == "entities")
                    fullPopulation += robotRd.lastConfig_.size();

                if (itArea->second->getEntityType() == "objects" || itArea->second->getEntityType() == "entities")
                    fullPopulation += objectRd.lastConfig_.size();

                if (fullPopulation == 0)
                    areaDensity = 0;
                else
                    areaDensity /= fullPopulation;

                if (ownerEnt != NULL) {
                    fact_msg.subjectOwnerId = ownerEnt->getId();
                }

                //Fact Density
                fact_msg.property = "AreaDensity";
                fact_msg.propertyType = "density";
                fact_msg.subProperty = "ratio";
                fact_msg.subjectId = itArea->first;
                fact_msg.targetId = "";
                fact_msg.confidence = 1.0;
                fact_msg.doubleValue = areaDensity;
                fact_msg.valueType = 1;
                fact_msg.factObservability = 0.0;
                fact_msg.time = densityTime;

                factList_msg.factList.push_back(fact_msg);
            }// Density

        }// for all area

        if (publishingArea_) {
            setAreaMsg(areaList_msg);
            area_pub.publish(areaList_msg);
        }

        fact_pub.publish(factList_msg);

        ros::spinOnce();

        loop_rate.sleep();
    }
    return 0;
}
