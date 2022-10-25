﻿// This is an independent project of an individual developer. Dear PVS-Studio, please check it.
// PVS-Studio Static Code Analyzer for C, C++, C#, and Java: http://www.viva64.com

#include "animation_system.h"
#include "..\ragebot\aim.h"
#include "local_animations.h"


void resolver::initialize(player_t* e, adjust_data* record, const float& goal_feet_yaw, const float& pitch)
{
    player = e;
    player_record = record;

    original_goal_feet_yaw = math::normalize_yaw(goal_feet_yaw);
    original_pitch = math::normalize_pitch(pitch);
}

void resolver::reset()
{
    player = nullptr;
    player_record = nullptr;

    side = false;
    fake = false;

    was_first_bruteforce = false;
    was_second_bruteforce = false;

    original_goal_feet_yaw = 0.0f;
    original_pitch = 0.0f;
}


/*float flAngleMod(float flAngle)
{
    return((360.0f / 65536.0f) * ((int32_t)(flAngle * (65536.0f / 360.0f)) & 65535));
}*/

float ApproachAngle(float flTarget, float flValue, float flSpeed)
{
   // flTarget = flAngleMod(flTarget);
   // flValue = flAngleMod(flValue);

    float delta = flTarget - flValue;

    if (flSpeed < 0)
        flSpeed = -flSpeed;

    if (delta < -180)
        delta += 360;
    else if (delta > 180)
        delta -= 360;

    if (delta > flSpeed)
        flValue += flSpeed;
    else if (delta < -flSpeed)
        flValue -= flSpeed;
    else
        flValue = flTarget;

    return flValue;
}

static auto GetSmoothedVelocity = [](float min_delta, Vector a, Vector b) {
    Vector delta = a - b;
    float delta_length = delta.Length();

    if (delta_length <= min_delta) {
        Vector result;
        if (-min_delta <= delta_length) {
            return a;
        }
        else {
            float iradius = 1.0f / (delta_length + FLT_EPSILON);
            return b - ((delta * iradius) * min_delta);
        }
    }
    else {
        float iradius = 1.0f / (delta_length + FLT_EPSILON);
        return b + ((delta * iradius) * min_delta);
    }
};


bool resolver::DoesHaveJitter(player_t* player, int* new_side)
{
    static float LastAngle[64];
    static int LastBrute[64];
    static bool Switch[64];
    static float LastUpdateTime[64];

    if (!math::IsNearEqual(player->m_angEyeAngles().y, LastAngle[player->EntIndex()], 60.f))
    {
        Switch[player->EntIndex()] = !Switch[player->EntIndex()];
        LastAngle[player->EntIndex()] = player->m_angEyeAngles().y;
        *new_side = Switch[player->EntIndex()] ? 1 : -1;
        LastBrute[player->EntIndex()] = *new_side;
        LastUpdateTime[player->EntIndex()] = m_globals()->m_curtime;
        return true;
    }
    else
    {
        if (fabsf(LastUpdateTime[player->EntIndex()] - m_globals()->m_curtime >= TICKS_TO_TIME(17)) || player->m_flSimulationTime() != player->m_flOldSimulationTime())
            LastAngle[player->EntIndex()] = player->m_angEyeAngles().y;

        *new_side = LastBrute[player->EntIndex()];
    }

    return false;
}

void resolver::resolve_yaw(player_t* player)
{

    if (!player || !player->get_animation_state())
        return;

    auto animState = player->get_animation_state();

    if (!animState)
        return;

    Vector velocity = player->m_vecVelocity();
    float spd = velocity.LengthSqr();
    if (spd > std::powf(1.2f * 260.0f, 2.f)) {
        Vector velocity_normalized = velocity.Normalized();
        velocity = velocity_normalized * (1.2f * 260.0f);


    }

    float Resolveyaw = animState->m_flGoalFeetYaw;

    auto delta_time
        = fmaxf(m_globals()->m_curtime - animState->m_flLastClientSideAnimationUpdateTime, 0.f);

    float deltatime = fabs(delta_time);
    float stop_to_full_running_fraction = 0.f;
    bool is_standing = true;
    float v25 = std::clamp(player->m_flDuckAmount() + animState->m_fLandingDuckAdditiveSomething, 0.0f, 1.0f);
    float v26 = animState->m_fDuckAmount;
    float v27 = deltatime * 6.0f;
    float v28;

    // clamp
    if ((v25 - v26) <= v27) {
        if (-v27 <= (v25 - v26))
            v28 = v25;
        else
            v28 = v26 - v27;
    }
    else {
        v28 = v26 + v27;
    }

    float flDuckAmount = std::clamp(v28, 0.0f, 1.0f);

    Vector animationVelocity = velocity;
    float speed = std::fminf(animationVelocity.Length(), 260.0f);

    auto weapon = player->m_hActiveWeapon().Get();
    if (!weapon)
        return;

    auto wpndata = weapon->get_csweapon_info();

    if (!wpndata)
        return;

    float flMaxMovementSpeed = 260.0f;
    if (weapon) {
        flMaxMovementSpeed = std::fmaxf(wpndata->flMaxPlayerSpeed, 0.001f);
    }

    float flRunningSpeed = speed / (flMaxMovementSpeed * 0.520f);
    float flDuckingSpeed_2 = speed / (flMaxMovementSpeed * 0.340f);

    flRunningSpeed = std::clamp(flRunningSpeed, 0.0f, 1.0f);

    float flYawModifier = (((stop_to_full_running_fraction * -0.3f) - 0.2f) * flRunningSpeed) + 1.0f;
    if (flDuckAmount > 0.0f) {
        float flDuckingSpeed = std::clamp(flDuckingSpeed_2, 0.0f, 1.0f);
        flYawModifier += (flDuckAmount * flDuckingSpeed) * (0.5f - flYawModifier);
    }

    float flMaxBodyYaw = *reinterpret_cast<float*>(&animState->pad10[512]);
    float flMinBodyYaw = *reinterpret_cast<float*>(&animState->pad10[516]);

    float flEyeYaw = player->m_angEyeAngles().y;

    float flEyeDiff = std::remainderf(flEyeYaw - Resolveyaw, 360.f);

    if (flEyeDiff <= flMaxBodyYaw) {
        if (flMinBodyYaw > flEyeDiff)
            Resolveyaw = fabs(flMinBodyYaw) + flEyeYaw;
    }
    else {
        Resolveyaw = flEyeYaw - fabs(flMaxBodyYaw);
    }

    if (speed > 0.1f || fabs(velocity.z) > 100.0f) {
        Resolveyaw = ApproachAngle(
            flEyeYaw,
            Resolveyaw,
            ((stop_to_full_running_fraction * 20.0f) + 30.0f)
            * deltatime);
    }
    else {
        Resolveyaw = ApproachAngle(
            player->m_flLowerBodyYawTarget(),
            Resolveyaw,
            deltatime * 100.0f);
    }

    if (stop_to_full_running_fraction > 0.0 && stop_to_full_running_fraction < 1.0)
    {
        const auto interval = m_globals()->m_intervalpertick * 2.f;

        if (is_standing)
            stop_to_full_running_fraction = stop_to_full_running_fraction - interval;
        else
            stop_to_full_running_fraction = interval + stop_to_full_running_fraction;

        stop_to_full_running_fraction = std::clamp(stop_to_full_running_fraction, 0.f, 1.f);
    }

    if (speed > 135.2f && is_standing)
    {
        stop_to_full_running_fraction = fmaxf(stop_to_full_running_fraction, .0099999998f);
        is_standing = false;
    }

    if (speed < 135.2f && !is_standing)
    {
        stop_to_full_running_fraction = fminf(stop_to_full_running_fraction, .99000001f);
        is_standing = true;
    }

    float Left = flEyeYaw + flMinBodyYaw;
    float Right = flEyeYaw + flMaxBodyYaw;
    float brute_yaw = Resolveyaw;
    brute_yaw = std::remainderf(brute_yaw, 360.f);
    if (c_config::get()->auto_check(c_config::get()->i["rage_aa_over"], c_config::get()->i["rage_aa_over_style"]))
    {
        switch (g_ctx.globals.missed_shots[player->EntIndex()] % 3)
        {
        case 0:
            player->get_animation_state()->m_flGoalFeetYaw = Right;
            break;
        case 1:
            player->get_animation_state()->m_flGoalFeetYaw = brute_yaw;
            break;
        case 2:
            player->get_animation_state()->m_flGoalFeetYaw = Left;
            break;
        default:
            break;
        }
    }
    else
    {
        switch (g_ctx.globals.missed_shots[player->EntIndex()] % 3)
        {
        case 0:
            player->get_animation_state()->m_flGoalFeetYaw = Left;
            break;
        case 1:
            player->get_animation_state()->m_flGoalFeetYaw = brute_yaw;
            break;
        case 2:
            player->get_animation_state()->m_flGoalFeetYaw = Right;
            break;
        default:
            break;
        }
    }
}

float resolver::resolve_pitch()
{
    player_t* player;

    return player->m_angEyeAngles().x;
   // return original_pitch;

}


void resolver::legitaa(player_t* player)
{
    auto animstate = player->get_animation_state();

    if (player->is_alive())
        return;

    if (player->is_player())
        return;

    //если первый шот мисс = брутим
    //вместо ротейта используя goalfeet, мы используем eyeangles
    switch (g_ctx.globals.missed_shots[player->EntIndex()] % 1)
    {
    case 0:
        animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y + 180.f);
        break;
    case 1:
        animstate->m_flGoalFeetYaw = math::normalize_yaw(player->m_angEyeAngles().y - 180.f);
        break;
    }
}



bool resolver::RollAA(player_t* ent)
{
    auto record = player_record;
    if (!g_ctx.local()->is_alive())
        return false;
    float angle_diff = math::normalize_yaw(0);
    Vector first = ZERO, second = ZERO, third = ZERO;
    first = Vector(player->hitbox_position(HITBOX_HEAD).x, player->hitbox_position(HITBOX_HEAD).y + min(angle_diff, 5), player->hitbox_position(HITBOX_HEAD).z);
    second = Vector(player->hitbox_position(HITBOX_HEAD).x, player->hitbox_position(HITBOX_HEAD).y, player->hitbox_position(HITBOX_HEAD).z);
    third = Vector(player->hitbox_position(HITBOX_HEAD).x, player->hitbox_position(HITBOX_HEAD).y - min(angle_diff, 5), player->hitbox_position(HITBOX_HEAD).z);
    Ray_t one, two, three;
    trace_t tone, ttwo, ttree;
    CTraceFilter fl;
    fl.pSkip = player;
    one.Init(g_ctx.local()->get_shoot_position(), first);
    two.Init(g_ctx.local()->get_shoot_position(), second);
    three.Init(g_ctx.local()->get_shoot_position(), third);
    m_trace()->TraceRay(one, MASK_PLAYERSOLID, &fl, &tone);
    m_trace()->TraceRay(two, MASK_PLAYERSOLID, &fl, &ttwo);
    m_trace()->TraceRay(three, MASK_PLAYERSOLID, &fl, &ttree);

    float lby = fabs(math::normalize_yaw(player->m_flLowerBodyYawTarget()));
    if (lby < 5 && lby > -5)
        return true;
    return false;
}



void resolver::good_resik(player_t* player)
{

}





