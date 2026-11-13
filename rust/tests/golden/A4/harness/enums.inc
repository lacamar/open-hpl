enum eLuxEnemyMessage
{
	eLuxEnemyMessage_TimeOut,
	eLuxEnemyMessage_TimeOut_2,
	eLuxEnemyMessage_TimeOut_3,
	eLuxEnemyMessage_TimeOut_4,

	eLuxEnemyMessage_StopPatrolAnimation,

	eLuxEnemyMessage_AnimationOver,
	eLuxEnemyMessage_AnimationSpecialEvent,
	eLuxEnemyMessage_EndOfPath,

	eLuxEnemyMessage_FoodInRange,

	eLuxEnemyMessage_PlayerSeen,
	eLuxEnemyMessage_PlayerUnseen,
	eLuxEnemyMessage_PlayerDetected,
	eLuxEnemyMessage_PlayerUndetected,
	eLuxEnemyMessage_SoundHeard,
	eLuxEnemyMessage_PlayerInRange,
	eLuxEnemyMessage_PlayerOutOfRange,
	eLuxEnemyMessage_PlayerDead,

	eLuxEnemyMessage_ChangePose,

	eLuxEnemyMessage_HelpMe,
	eLuxEnemyMessage_StuckAtDoor,

	eLuxEnemyMessage_TakeHit,
	eLuxEnemyMessage_Death,

	eLuxEnemyMessage_Reset,

	eLuxEnemyMessage_LastEnum
};

enum eLuxEnemyMusic
{
	eLuxEnemyMusic_Search,
	eLuxEnemyMusic_Attack,
	eLuxEnemyMusic_LastEnum
};

enum eLuxEnemyType
{
	eLuxEnemyType_Grunt,
	eLuxEnemyType_WaterLurker,
	eLuxEnemyType_ManPig,

	eLuxEnemyType_LastEnum
};
