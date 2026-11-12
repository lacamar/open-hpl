//! Raw Newton 2.36 bindings (single-precision `dFloat`): the C API functions and callback types the engine uses.
#![allow(non_camel_case_types, non_snake_case, non_upper_case_globals)]

use core::ffi::{c_char, c_int, c_uint, c_void};

use core::mem::offset_of;

#[repr(C)]
pub struct NewtonBody {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NewtonCollision {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NewtonJoint {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NewtonMaterial {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NewtonSceneProxy {
    _private: [u8; 0],
}

#[repr(C)]
pub struct NewtonWorld {
    _private: [u8; 0],
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct NewtonHingeSliderUpdateDesc {
    pub m_accel: f32,
    pub m_minFriction: f32,
    pub m_maxFriction: f32,
    pub m_timestep: f32,
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct NewtonJointRecord {
    pub m_attachmenMatrix_0: [[f32; 4]; 4],
    pub m_attachmenMatrix_1: [[f32; 4]; 4],
    pub m_minLinearDof: [f32; 3],
    pub m_maxLinearDof: [f32; 3],
    pub m_minAngularDof: [f32; 3],
    pub m_maxAngularDof: [f32; 3],
    pub m_attachBody_0: *const NewtonBody,
    pub m_attachBody_1: *const NewtonBody,
    pub m_extraParameters: [f32; 16],
    pub m_bodiesCollisionOn: c_int,
    pub m_descriptionType: [c_char; 32],
}

pub type NewtonApplyForceAndTorque =
    unsafe extern "C" fn(body: *mut NewtonBody, timestep: f32, threadIndex: i32);
pub type NewtonBodyIterator = unsafe extern "C" fn(body: *const NewtonBody, userData: *mut c_void);
pub type NewtonCollisionIterator = unsafe extern "C" fn(
    userData: *mut c_void,
    vertexCount: c_int,
    faceArray: *const f32,
    faceId: c_int,
);
pub type NewtonContactsProcess =
    unsafe extern "C" fn(contact: *const NewtonJoint, timestep: f32, threadIndex: i32);
pub type NewtonCorkscrewCallBack = unsafe extern "C" fn(
    corkscrew: *const NewtonJoint,
    desc: *mut NewtonHingeSliderUpdateDesc,
) -> c_uint;
pub type NewtonDeserialize =
    unsafe extern "C" fn(serializeHandle: *mut c_void, buffer: *mut c_void, size: usize);
pub type NewtonGetBuoyancyPlane = unsafe extern "C" fn(
    collisionID: i32,
    context: *mut c_void,
    globalSpaceMatrix: *const f32,
    globalSpacePlane: *mut f32,
) -> c_int;
pub type NewtonOnAABBOverlap = unsafe extern "C" fn(
    material: *const NewtonMaterial,
    body0: *const NewtonBody,
    body1: *const NewtonBody,
    threadIndex: i32,
) -> c_int;
pub type NewtonSerialize =
    unsafe extern "C" fn(serializeHandle: *mut c_void, buffer: *const c_void, size: usize);
pub type NewtonSetTransform =
    unsafe extern "C" fn(body: *const NewtonBody, matrix: *const f32, threadIndex: i32);
pub type NewtonSliderCallBack = unsafe extern "C" fn(
    slider: *const NewtonJoint,
    desc: *mut NewtonHingeSliderUpdateDesc,
) -> c_uint;
pub type NewtonUserBilateralCallBack =
    unsafe extern "C" fn(userJoint: *const NewtonJoint, timestep: f32, threadIndex: c_int);
pub type NewtonUserBilateralGetInfoCallBack =
    unsafe extern "C" fn(userJoint: *const NewtonJoint, info: *mut NewtonJointRecord);
pub type NewtonWorldRayFilterCallback = unsafe extern "C" fn(
    body: *const NewtonBody,
    hitNormal: *const f32,
    collisionID: c_int,
    userData: *mut c_void,
    intersectParam: f32,
) -> f32;
pub type NewtonWorldRayPrefilterCallback = unsafe extern "C" fn(
    body: *const NewtonBody,
    collision: *const NewtonCollision,
    userData: *mut c_void,
) -> c_uint;

unsafe extern "C" {
    pub fn NewtonCreate() -> *mut NewtonWorld;
    pub fn NewtonDestroy(newtonWorld: *const NewtonWorld);
    pub fn NewtonUpdate(newtonWorld: *mut NewtonWorld, timestep: f32);
    pub fn NewtonSetSolverModel(newtonWorld: *mut NewtonWorld, model: c_int);
    pub fn NewtonWorldCriticalSectionLock(newtonWorld: *mut NewtonWorld);
    pub fn NewtonWorldCriticalSectionUnlock(newtonWorld: *mut NewtonWorld);
    pub fn NewtonSetThreadsCount(newtonWorld: *mut NewtonWorld, threads: c_int);
    pub fn NewtonGetThreadsCount(newtonWorld: *const NewtonWorld) -> c_int;
    pub fn NewtonSetFrictionModel(newtonWorld: *mut NewtonWorld, model: c_int);
    pub fn NewtonSetWorldSize(
        newtonWorld: *mut NewtonWorld,
        minPoint: *const f32,
        maxPoint: *const f32,
    );
    pub fn NewtonWorldForEachBodyInAABBDo(
        newtonWorld: *const NewtonWorld,
        p0: *const f32,
        p1: *const f32,
        callback: Option<NewtonBodyIterator>,
        userData: *mut c_void,
    );
    pub fn NewtonWorldRayCast(
        newtonWorld: *mut NewtonWorld,
        p0: *const f32,
        p1: *const f32,
        filter: Option<NewtonWorldRayFilterCallback>,
        userData: *mut c_void,
        prefilter: Option<NewtonWorldRayPrefilterCallback>,
    );
    pub fn NewtonMaterialCreateGroupID(newtonWorld: *mut NewtonWorld) -> c_int;
    pub fn NewtonMaterialGetDefaultGroupID(newtonWorld: *const NewtonWorld) -> c_int;
    pub fn NewtonMaterialSetContinuousCollisionMode(
        newtonWorld: *const NewtonWorld,
        id0: c_int,
        id1: c_int,
        state: c_int,
    );
    pub fn NewtonMaterialSetCollisionCallback(
        newtonWorld: *const NewtonWorld,
        id0: c_int,
        id1: c_int,
        userData: *mut c_void,
        aabbOverlap: Option<NewtonOnAABBOverlap>,
        process: Option<NewtonContactsProcess>,
    );
    pub fn NewtonMaterialSetDefaultElasticity(
        newtonWorld: *const NewtonWorld,
        id0: c_int,
        id1: c_int,
        elasticCoef: f32,
    );
    pub fn NewtonMaterialSetDefaultFriction(
        newtonWorld: *const NewtonWorld,
        id0: c_int,
        id1: c_int,
        staticFriction: f32,
        kineticFriction: f32,
    );
    pub fn NewtonMaterialGetContactNormalSpeed(material: *const NewtonMaterial) -> f32;
    pub fn NewtonMaterialGetContactForce(
        material: *const NewtonMaterial,
        body: *mut NewtonBody,
        force: *mut f32,
    );
    pub fn NewtonMaterialGetContactPositionAndNormal(
        material: *const NewtonMaterial,
        body: *mut NewtonBody,
        posit: *mut f32,
        normal: *mut f32,
    );
    pub fn NewtonMaterialGetContactTangentSpeed(
        material: *const NewtonMaterial,
        index: c_int,
    ) -> f32;
    pub fn NewtonCreateNull(newtonWorld: *mut NewtonWorld) -> *mut NewtonCollision;
    pub fn NewtonCreateSphere(
        newtonWorld: *mut NewtonWorld,
        radiusX: f32,
        radiusY: f32,
        radiusZ: f32,
        shapeID: c_int,
        offsetMatrix: *const f32,
    ) -> *mut NewtonCollision;
    pub fn NewtonCreateBox(
        newtonWorld: *mut NewtonWorld,
        dx: f32,
        dy: f32,
        dz: f32,
        shapeID: c_int,
        offsetMatrix: *const f32,
    ) -> *mut NewtonCollision;
    pub fn NewtonCreateCapsule(
        newtonWorld: *mut NewtonWorld,
        radius: f32,
        height: f32,
        shapeID: c_int,
        offsetMatrix: *const f32,
    ) -> *mut NewtonCollision;
    pub fn NewtonCreateCylinder(
        newtonWorld: *mut NewtonWorld,
        radius: f32,
        height: f32,
        shapeID: c_int,
        offsetMatrix: *const f32,
    ) -> *mut NewtonCollision;
    pub fn NewtonConvexCollisionCalculateInertialMatrix(
        convexCollision: *const NewtonCollision,
        inertia: *mut f32,
        origin: *mut f32,
    );
    pub fn NewtonReleaseCollision(newtonWorld: *mut NewtonWorld, collision: *mut NewtonCollision);
    pub fn NewtonCreateCompoundCollision(
        newtonWorld: *mut NewtonWorld,
        count: c_int,
        collisionPrimitiveArray: *const *mut NewtonCollision,
        shapeID: c_int,
    ) -> *mut NewtonCollision;
    pub fn NewtonCreateSceneCollision(
        newtonWorld: *mut NewtonWorld,
        shapeID: c_int,
    ) -> *mut NewtonCollision;
    pub fn NewtonSceneCollisionCreateProxy(
        scene: *mut NewtonCollision,
        collision: *mut NewtonCollision,
        matrix: *mut f32,
    ) -> *mut NewtonSceneProxy;
    pub fn NewtonSceneProxySetMatrix(proxy: *mut NewtonSceneProxy, matrix: *const f32);
    pub fn NewtonSceneCollisionOptimize(scene: *mut NewtonCollision);
    pub fn NewtonCreateCollisionFromSerialization(
        newtonWorld: *mut NewtonWorld,
        deserializeFunction: Option<NewtonDeserialize>,
        serializeHandle: *mut c_void,
    ) -> *mut NewtonCollision;
    pub fn NewtonCollisionSerialize(
        newtonWorld: *const NewtonWorld,
        collision: *const NewtonCollision,
        serializeFunction: Option<NewtonSerialize>,
        serializeHandle: *mut c_void,
    );
    pub fn NewtonCreateHeightFieldCollision(
        newtonWorld: *mut NewtonWorld,
        width: c_int,
        height: c_int,
        gridsDiagonals: c_int,
        elevationMap: *const u16,
        attributeMap: *const i8,
        horizontalScale: f32,
        verticalScale: f32,
        shapeID: c_int,
    ) -> *mut NewtonCollision;
    pub fn NewtonCreateTreeCollision(
        newtonWorld: *mut NewtonWorld,
        shapeID: c_int,
    ) -> *mut NewtonCollision;
    pub fn NewtonTreeCollisionBeginBuild(treeCollision: *mut NewtonCollision);
    pub fn NewtonTreeCollisionAddFace(
        treeCollision: *mut NewtonCollision,
        vertexCount: c_int,
        vertexPtr: *const f32,
        strideInBytes: c_int,
        faceAttribute: c_int,
    );
    pub fn NewtonTreeCollisionEndBuild(treeCollision: *mut NewtonCollision, optimize: c_int);
    pub fn NewtonCollisionCollide(
        newtonWorld: *mut NewtonWorld,
        maxSize: c_int,
        collisionA: *mut NewtonCollision,
        matrixA: *const f32,
        collisionB: *mut NewtonCollision,
        matrixB: *const f32,
        contacts: *mut f32,
        normals: *mut f32,
        penetration: *mut f32,
        threadIndex: c_int,
    ) -> c_int;
    pub fn NewtonCollisionRayCast(
        collision: *const NewtonCollision,
        p0: *const f32,
        p1: *const f32,
        normal: *mut f32,
        attribute: *mut c_int,
    ) -> f32;
    pub fn NewtonCollisionForEachPolygonDo(
        collision: *const NewtonCollision,
        matrix: *const f32,
        callback: Option<NewtonCollisionIterator>,
        userData: *mut c_void,
    );
    pub fn NewtonCreateBody(
        newtonWorld: *mut NewtonWorld,
        collision: *mut NewtonCollision,
        matrix: *const f32,
    ) -> *mut NewtonBody;
    pub fn NewtonDestroyBody(newtonWorld: *mut NewtonWorld, body: *mut NewtonBody);
    pub fn NewtonBodyAddForce(body: *mut NewtonBody, force: *const f32);
    pub fn NewtonBodyAddTorque(body: *mut NewtonBody, torque: *const f32);
    pub fn NewtonBodySetMatrix(body: *mut NewtonBody, matrix: *mut f32);
    pub fn NewtonBodySetMassMatrix(body: *mut NewtonBody, mass: f32, Ixx: f32, Iyy: f32, Izz: f32);
    pub fn NewtonBodySetMaterialGroupID(body: *mut NewtonBody, id: c_int);
    pub fn NewtonBodySetContinuousCollisionMode(body: *mut NewtonBody, state: c_uint);
    pub fn NewtonBodySetOmega(body: *mut NewtonBody, omega: *const f32);
    pub fn NewtonBodySetVelocity(body: *mut NewtonBody, velocity: *const f32);
    pub fn NewtonBodySetCentreOfMass(body: *mut NewtonBody, com: *const f32);
    pub fn NewtonBodySetLinearDamping(body: *mut NewtonBody, linearDamp: f32);
    pub fn NewtonBodySetAngularDamping(body: *mut NewtonBody, angularDamp: *const f32);
    pub fn NewtonBodySetUserData(body: *mut NewtonBody, userData: *mut c_void);
    pub fn NewtonBodyGetSleepState(body: *const NewtonBody) -> c_int;
    pub fn NewtonBodyGetAutoSleep(body: *const NewtonBody) -> c_int;
    pub fn NewtonBodySetAutoSleep(body: *mut NewtonBody, state: c_int);
    pub fn NewtonBodySetFreezeState(body: *mut NewtonBody, state: c_int);
    pub fn NewtonBodySetTransformCallback(
        body: *mut NewtonBody,
        callback: Option<NewtonSetTransform>,
    );
    pub fn NewtonBodySetForceAndTorqueCallback(
        body: *mut NewtonBody,
        callback: Option<NewtonApplyForceAndTorque>,
    );
    pub fn NewtonBodyGetUserData(body: *const NewtonBody) -> *mut c_void;
    pub fn NewtonBodyGetWorld(body: *const NewtonBody) -> *mut NewtonWorld;
    pub fn NewtonBodyGetContinuousCollisionMode(body: *const NewtonBody) -> c_int;
    pub fn NewtonBodyGetMassMatrix(
        body: *const NewtonBody,
        mass: *mut f32,
        Ixx: *mut f32,
        Iyy: *mut f32,
        Izz: *mut f32,
    );
    pub fn NewtonBodyGetOmega(body: *const NewtonBody, vector: *mut f32);
    pub fn NewtonBodyGetVelocity(body: *const NewtonBody, vector: *mut f32);
    pub fn NewtonBodyGetForce(body: *const NewtonBody, vector: *mut f32);
    pub fn NewtonBodyGetCentreOfMass(body: *const NewtonBody, com: *mut f32);
    pub fn NewtonBodyGetLinearDamping(body: *const NewtonBody) -> f32;
    pub fn NewtonBodyGetAngularDamping(body: *const NewtonBody, vector: *mut f32);
    pub fn NewtonContactJointGetFirstContact(contactJoint: *const NewtonJoint) -> *mut c_void;
    pub fn NewtonContactJointGetNextContact(
        contactJoint: *const NewtonJoint,
        contact: *mut c_void,
    ) -> *mut c_void;
    pub fn NewtonContactGetMaterial(contact: *mut c_void) -> *mut NewtonMaterial;
    pub fn NewtonBodyAddBuoyancyForce(
        body: *mut NewtonBody,
        fluidDensity: f32,
        fluidLinearViscosity: f32,
        fluidAngularViscosity: f32,
        gravityVector: *const f32,
        buoyancyPlane: Option<NewtonGetBuoyancyPlane>,
        context: *mut c_void,
    );
    pub fn NewtonBodyAddImpulse(
        body: *mut NewtonBody,
        pointDeltaVeloc: *const f32,
        pointPosit: *const f32,
    );
    pub fn NewtonJointGetUserData(joint: *const NewtonJoint) -> *mut c_void;
    pub fn NewtonJointSetUserData(joint: *const NewtonJoint, userData: *mut c_void);
    pub fn NewtonJointGetBody0(joint: *const NewtonJoint) -> *mut NewtonBody;
    pub fn NewtonJointGetBody1(joint: *const NewtonJoint) -> *mut NewtonBody;
    pub fn NewtonJointGetCollisionState(joint: *const NewtonJoint) -> c_int;
    pub fn NewtonJointSetCollisionState(joint: *mut NewtonJoint, state: c_int);
    pub fn NewtonJointGetStiffness(joint: *const NewtonJoint) -> f32;
    pub fn NewtonJointSetStiffness(joint: *mut NewtonJoint, state: f32);
    pub fn NewtonDestroyJoint(newtonWorld: *mut NewtonWorld, joint: *mut NewtonJoint);
    pub fn NewtonConstraintCreateBall(
        newtonWorld: *mut NewtonWorld,
        pivotPoint: *const f32,
        childBody: *mut NewtonBody,
        parentBody: *mut NewtonBody,
    ) -> *mut NewtonJoint;
    pub fn NewtonBallGetJointAngle(ball: *const NewtonJoint, angle: *mut f32);
    pub fn NewtonBallGetJointOmega(ball: *const NewtonJoint, omega: *mut f32);
    pub fn NewtonBallGetJointForce(ball: *const NewtonJoint, force: *mut f32);
    pub fn NewtonBallSetConeLimits(
        ball: *mut NewtonJoint,
        pin: *const f32,
        maxConeAngle: f32,
        maxTwistAngle: f32,
    );
    pub fn NewtonHingeGetJointAngle(hinge: *const NewtonJoint) -> f32;
    pub fn NewtonHingeCalculateStopAlpha(
        hinge: *const NewtonJoint,
        desc: *const NewtonHingeSliderUpdateDesc,
        angle: f32,
    ) -> f32;
    pub fn NewtonConstraintCreateSlider(
        newtonWorld: *mut NewtonWorld,
        pivotPoint: *const f32,
        pinDir: *const f32,
        childBody: *mut NewtonBody,
        parentBody: *mut NewtonBody,
    ) -> *mut NewtonJoint;
    pub fn NewtonSliderSetUserCallback(
        slider: *mut NewtonJoint,
        callback: Option<NewtonSliderCallBack>,
    );
    pub fn NewtonSliderGetJointPosit(slider: *const NewtonJoint) -> f32;
    pub fn NewtonSliderGetJointVeloc(slider: *const NewtonJoint) -> f32;
    pub fn NewtonSliderGetJointForce(slider: *const NewtonJoint, force: *mut f32);
    pub fn NewtonSliderCalculateStopAccel(
        slider: *const NewtonJoint,
        desc: *const NewtonHingeSliderUpdateDesc,
        position: f32,
    ) -> f32;
    pub fn NewtonConstraintCreateCorkscrew(
        newtonWorld: *mut NewtonWorld,
        pivotPoint: *const f32,
        pinDir: *const f32,
        childBody: *mut NewtonBody,
        parentBody: *mut NewtonBody,
    ) -> *mut NewtonJoint;
    pub fn NewtonCorkscrewSetUserCallback(
        corkscrew: *mut NewtonJoint,
        callback: Option<NewtonCorkscrewCallBack>,
    );
    pub fn NewtonCorkscrewGetJointPosit(corkscrew: *const NewtonJoint) -> f32;
    pub fn NewtonCorkscrewGetJointAngle(corkscrew: *const NewtonJoint) -> f32;
    pub fn NewtonCorkscrewGetJointVeloc(corkscrew: *const NewtonJoint) -> f32;
    pub fn NewtonCorkscrewGetJointForce(corkscrew: *const NewtonJoint, force: *mut f32);
    pub fn NewtonCorkscrewCalculateStopAccel(
        corkscrew: *const NewtonJoint,
        desc: *const NewtonHingeSliderUpdateDesc,
        position: f32,
    ) -> f32;
    pub fn NewtonConstraintCreateUserJoint(
        newtonWorld: *const NewtonWorld,
        maxDOF: c_int,
        callback: Option<NewtonUserBilateralCallBack>,
        getInfo: Option<NewtonUserBilateralGetInfoCallBack>,
        childBody: *const NewtonBody,
        parentBody: *const NewtonBody,
    ) -> *mut NewtonJoint;
    pub fn NewtonUserJointAddLinearRow(
        joint: *const NewtonJoint,
        pivot0: *const f32,
        pivot1: *const f32,
        dir: *const f32,
    );
    pub fn NewtonUserJointAddAngularRow(
        joint: *const NewtonJoint,
        relativeAngle: f32,
        dir: *const f32,
    );
    pub fn NewtonUserJointSetRowMinimumFriction(joint: *const NewtonJoint, friction: f32);
    pub fn NewtonUserJointSetRowMaximumFriction(joint: *const NewtonJoint, friction: f32);
    pub fn NewtonUserJointSetRowStiffness(joint: *const NewtonJoint, stiffness: f32);
    pub fn NewtonUserJointGetRowForce(joint: *const NewtonJoint, row: c_int) -> f32;
}

const _: () = {
    assert!(size_of::<NewtonHingeSliderUpdateDesc>() == 16);
    assert!(align_of::<NewtonHingeSliderUpdateDesc>() == 4);
    assert!(offset_of!(NewtonHingeSliderUpdateDesc, m_accel) == 0);
    assert!(offset_of!(NewtonHingeSliderUpdateDesc, m_minFriction) == 4);
    assert!(offset_of!(NewtonHingeSliderUpdateDesc, m_maxFriction) == 8);
    assert!(offset_of!(NewtonHingeSliderUpdateDesc, m_timestep) == 12);
    assert!(size_of::<NewtonJointRecord>() == 296);
    assert!(align_of::<NewtonJointRecord>() == 8);
    assert!(offset_of!(NewtonJointRecord, m_attachmenMatrix_0) == 0);
    assert!(offset_of!(NewtonJointRecord, m_attachmenMatrix_1) == 64);
    assert!(offset_of!(NewtonJointRecord, m_minLinearDof) == 128);
    assert!(offset_of!(NewtonJointRecord, m_maxLinearDof) == 140);
    assert!(offset_of!(NewtonJointRecord, m_minAngularDof) == 152);
    assert!(offset_of!(NewtonJointRecord, m_maxAngularDof) == 164);
    assert!(offset_of!(NewtonJointRecord, m_attachBody_0) == 176);
    assert!(offset_of!(NewtonJointRecord, m_attachBody_1) == 184);
    assert!(offset_of!(NewtonJointRecord, m_extraParameters) == 192);
    assert!(offset_of!(NewtonJointRecord, m_bodiesCollisionOn) == 256);
    assert!(offset_of!(NewtonJointRecord, m_descriptionType) == 260);
};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn layout() {
        assert_eq!(size_of::<NewtonHingeSliderUpdateDesc>(), 16);
        assert_eq!(align_of::<NewtonHingeSliderUpdateDesc>(), 4);
        assert_eq!(offset_of!(NewtonHingeSliderUpdateDesc, m_accel), 0);
        assert_eq!(offset_of!(NewtonHingeSliderUpdateDesc, m_minFriction), 4);
        assert_eq!(offset_of!(NewtonHingeSliderUpdateDesc, m_maxFriction), 8);
        assert_eq!(offset_of!(NewtonHingeSliderUpdateDesc, m_timestep), 12);
        assert_eq!(size_of::<NewtonJointRecord>(), 296);
        assert_eq!(align_of::<NewtonJointRecord>(), 8);
        assert_eq!(offset_of!(NewtonJointRecord, m_attachmenMatrix_0), 0);
        assert_eq!(offset_of!(NewtonJointRecord, m_attachmenMatrix_1), 64);
        assert_eq!(offset_of!(NewtonJointRecord, m_minLinearDof), 128);
        assert_eq!(offset_of!(NewtonJointRecord, m_maxLinearDof), 140);
        assert_eq!(offset_of!(NewtonJointRecord, m_minAngularDof), 152);
        assert_eq!(offset_of!(NewtonJointRecord, m_maxAngularDof), 164);
        assert_eq!(offset_of!(NewtonJointRecord, m_attachBody_0), 176);
        assert_eq!(offset_of!(NewtonJointRecord, m_attachBody_1), 184);
        assert_eq!(offset_of!(NewtonJointRecord, m_extraParameters), 192);
        assert_eq!(offset_of!(NewtonJointRecord, m_bodiesCollisionOn), 256);
        assert_eq!(offset_of!(NewtonJointRecord, m_descriptionType), 260);
    }
}
