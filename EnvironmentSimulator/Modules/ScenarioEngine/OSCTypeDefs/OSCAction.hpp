/*
 * esmini - Environment Simulator Minimalistic
 * https://github.com/esmini/esmini
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) partners of Simulation Scenarios
 * https://sites.google.com/view/simulationscenarios
 */

#pragma once

#include "Entities.hpp"

namespace scenarioengine
{

    class StoryBoardElement
    {
    public:
        static void (*stateChangeCallback)(const char* name, int type, int state);

        /**
         * Take note, changing this enum will alter the public API in esminiLib.hpp
         */
        typedef enum
        {
            STORY_BOARD            = 1,
            STORY                  = 2,
            ACT                    = 3,
            MANEUVER_GROUP         = 4,
            MANEUVER               = 5,
            EVENT                  = 6,
            ACTION                 = 7,
            UNDEFINED_ELEMENT_TYPE = 0
        } ElementType;

        /**
         * Take note, changing this enum will alter the public API in esminiLib.hpp
         */
        typedef enum
        {
            STANDBY                 = 1,
            RUNNING                 = 2,
            COMPLETE                = 3,
            UNDEFINED_ELEMENT_STATE = 0
        } State;

        typedef enum
        {
            START_TRANSITION,  // Transitions last for one step
            END_TRANSITION,
            STOP_TRANSITION,
            SKIP_TRANSITION,
            UNDEFINED_ELEMENT_TRANSITION
        } Transition;

        ElementType type_;
        State       state_;
        Transition  transition_;
        std::string name_;
        int         num_executions_;
        int         max_num_executions_;
        bool        set_flag_;  // indicate state changed current timestep, keep transition next step

        StoryBoardElement(ElementType type)
            : type_(type),
              state_(State::STANDBY),
              transition_(Transition::UNDEFINED_ELEMENT_TRANSITION),
              num_executions_(0),
              max_num_executions_(-1),
              set_flag_(false)
        {
        }

        StoryBoardElement(ElementType type, int max_num_executions)
            : type_(type),
              state_(State::STANDBY),
              transition_(Transition::UNDEFINED_ELEMENT_TRANSITION),
              num_executions_(0),
              max_num_executions_(max_num_executions),
              set_flag_(false)
        {
        }

        virtual ~StoryBoardElement() = default;

        virtual void UpdateState();
        void         SetState(State state);
        std::string  state2str(State state);
        std::string  transition2str(StoryBoardElement::Transition state);

        bool IsActive()
        {
            // Also consider when just being activated - indicated by next_state == running
            // to avoid single frames of no updates (zero motion)
            // Elements on transition to end or stop states also considered not active
            return ((state_ == State::RUNNING) && (transition_ != Transition::END_TRANSITION && transition_ != Transition::STOP_TRANSITION));
        }

        bool IsTriggable()
        {
            return state_ == State::STANDBY;
        }

        virtual void Start(double simTime, double dt)
        {
            (void)simTime;
            (void)dt;
            if (state_ == State::STANDBY)
            {
                transition_ = Transition::START_TRANSITION;
                SetState(State::RUNNING);
                num_executions_++;
            }
            else
            {
                LOG("%s Invalid Start transition request from %s to %s", name_.c_str(), state2str(state_).c_str(), state2str(State::RUNNING).c_str());
            }
        }

        virtual void Stop()
        {
            if (state_ == State::STANDBY || state_ == State::RUNNING)
            {
                transition_ = Transition::STOP_TRANSITION;
                SetState(state_ = State::COMPLETE);
            }
            else
            {
                LOG("%s Invalid Stop transition requested from %s to %s",
                    name_.c_str(),
                    state2str(state_).c_str(),
                    state2str(State::COMPLETE).c_str());
            }
        }

        virtual void End(double simTime)
        {
            (void)simTime;
            // Allow elements to move directly from standby to complete
            // Some actions are atomic, and don't need run time
            if (state_ == State::RUNNING || state_ == State::STANDBY)
            {
                transition_ = Transition::END_TRANSITION;

                if (type_ == ElementType::MANEUVER_GROUP || type_ == ElementType::EVENT)
                {
                    if (max_num_executions_ != -1 && num_executions_ >= max_num_executions_)
                    {
                        LOG("%s complete after %d execution%s", name_.c_str(), num_executions_, num_executions_ > 1 ? "s" : "");
                        SetState(State::COMPLETE);
                    }
                    else
                    {
                        SetState(State::STANDBY);
                    }
                }
                else
                {
                    SetState(State::COMPLETE);  // no number_of_execution attribute, just execute once
                }
            }
            else
            {
                LOG("%s Invalid End transition requested from %s to %s or %s",
                    name_.c_str(),
                    state2str(state_).c_str(),
                    state2str(State::STANDBY).c_str(),
                    state2str(State::COMPLETE).c_str());
            }
        }

        virtual bool IsComplete()
        {
            return false;
        }

        void Standby()
        {
            if (state_ == State::STANDBY)
            {
                transition_ = Transition::SKIP_TRANSITION;
                SetState(State::STANDBY);
            }
            else if (state_ == State::RUNNING)
            {
                transition_ = Transition::END_TRANSITION;
                SetState(State::STANDBY);
            }
            else
            {
                LOG("Invalid transition requested from %s to %s", state2str(state_).c_str(), state2str(State::STANDBY).c_str());
            }
        }

        void Reset()
        {
            state_          = State::STANDBY;
            transition_     = Transition::UNDEFINED_ELEMENT_TRANSITION;
            num_executions_ = 0;
            set_flag_       = false;
        }
    };

    class OSCAction : public StoryBoardElement
    {
    public:
        typedef enum
        {
            GLOBAL,
            USER_DEFINED,
            PRIVATE,
        } BaseType;

        BaseType base_type_;

        OSCAction(BaseType type) : StoryBoardElement(StoryBoardElement::ElementType::ACTION), base_type_(type)
        {
        }

        virtual ~OSCAction()
        {
        }

        std::string         BaseType2Str();
        virtual std::string Type2Str()
        {
            return BaseType2Str();
        }

        virtual void Step(double simTime, double dt) = 0;
        bool         IsComplete() override;
    };

    class OSCUserDefinedAction : public OSCAction
    {
    public:
        OSCUserDefinedAction() : OSCAction(OSCAction::BaseType::USER_DEFINED)
        {
        }

        OSCUserDefinedAction(const OSCUserDefinedAction& action) : OSCAction(OSCAction::BaseType::USER_DEFINED)
        {
            name_    = action.name_;
            type_    = action.type_;
            content_ = action.content_;
        }

        ~OSCUserDefinedAction()
        {
        }

        OSCUserDefinedAction* Copy()
        {
            OSCUserDefinedAction* new_action = new OSCUserDefinedAction(*this);
            return new_action;
        }

        std::string Type2Str()
        {
            return "UserDefinedAction";
        };

        void Start(double simTime, double dt)
        {
            LOG("Starting %s type: %s content: %s", Type2Str().c_str(), type_.c_str(), content_.c_str());
            OSCAction::Start(simTime, dt);
        }

        void Step(double, double)
        {
        }

        std::string type_;
        std::string content_;
    };

}  // namespace scenarioengine
